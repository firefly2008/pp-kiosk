// SPDX-License-Identifier: GPL-2.0-or-later
#include "appiconprovider.h"

#include <QUrl>

#ifdef Q_OS_WIN
#include <windows.h>
#include <commctrl.h>
#include <commoncontrols.h>
#include <shellapi.h>
#include <shlobj.h>
#endif

#ifdef Q_OS_WIN
static bool contentIsTiny(const QImage &source)
{
    const QImage image = source.convertToFormat(QImage::Format_ARGB32);
    const int limit = image.width() / 3;
    for (int y = 0; y < image.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = (y < limit ? limit : 0); x < image.width(); ++x) {
            if (qAlpha(line[x]) > 16) {
                return false;
            }
        }
    }
    return true;
}
#endif

AppIconProvider::AppIconProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage AppIconProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    QImage image;
#ifdef Q_OS_WIN
    const QString path = QUrl::fromPercentEncoding(id.toLatin1());
    const HRESULT comInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    SHFILEINFOW info{};
    if (SHGetFileInfoW(reinterpret_cast<LPCWSTR>(path.utf16()), 0, &info, sizeof(info), SHGFI_SYSICONINDEX)) {
        // Grootste beschikbare formaat eerst (256 px), anders 48 px
        for (int listId : {SHIL_JUMBO, SHIL_EXTRALARGE}) {
            IImageList *list = nullptr;
            if (FAILED(SHGetImageList(listId, IID_IImageList, reinterpret_cast<void **>(&list))) || !list) {
                continue;
            }
            HICON icon = nullptr;
            if (SUCCEEDED(list->GetIcon(info.iIcon, ILD_TRANSPARENT, &icon)) && icon) {
                image = QImage::fromHICON(icon);
                DestroyIcon(icon);
            }
            list->Release();
            // Programma's zonder groot pictogram leveren een klein plaatje in de hoek
            // van een leeg 256px-vlak: dan liever het 48px-formaat gebruiken.
            if (listId == SHIL_JUMBO && !image.isNull() && contentIsTiny(image)) {
                image = QImage();
            }
            if (!image.isNull()) {
                break;
            }
        }
    }
    if (SUCCEEDED(comInit)) {
        CoUninitialize();
    }
#else
    Q_UNUSED(id);
#endif
    if (image.isNull()) {
        // Niet gevonden: QML toont dan de beginletter van de tegel
        return image;
    }
    if (requestedSize.isValid() && requestedSize.width() > 0 && requestedSize.height() > 0) {
        image = image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    if (size) {
        *size = image.size();
    }
    return image;
}
