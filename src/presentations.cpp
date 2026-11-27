// SPDX-License-Identifier: GPL-2.0-or-later
#include "presentations.h"
#include "paths.h"

#include <QCollator>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPdfDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

namespace {
const QStringList imageFilters = {QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"), QStringLiteral("*.png"), QStringLiteral("*.webp"), QStringLiteral("*.bmp")};

QStringList sortedImages(const QDir &dir)
{
    QStringList files = dir.entryList(imageFilters, QDir::Files);
    QCollator collator;
    collator.setNumericMode(true);
    std::sort(files.begin(), files.end(), collator);
    return files;
}

QJsonObject readInfo(const QDir &dir)
{
    QFile f(dir.filePath(QStringLiteral("info.json")));
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(f.readAll()).object();
}
} // namespace

Presentations::Presentations(const QString &dataDir, QObject *parent)
    : QObject(parent)
    , m_root(dataDir + QStringLiteral("/presentaties"))
{
    QDir().mkpath(m_root);
    removeDemo();
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(800);
    connect(&m_debounce, &QTimer::timeout, this, &Presentations::refresh);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_debounce, qOverload<>(&QTimer::start));
    refresh();
}

void Presentations::removeDemo()
{
    // Vanaf 0.10: de voorbeeldpresentatie "Diner van de dag" is weg (het menubord laat het eten al zien)
    QSettings settings(appSettingsFile(), QSettings::IniFormat);
    if (settings.value(QStringLiteral("presentations/demoRemoved")).toBool()) {
        return;
    }
    QDir dir(m_root + QStringLiteral("/Diner van de dag"));
    if (dir.exists() && dir.removeRecursively()) {
        qInfo() << "Presentations: demo removed";
    }
    settings.setValue(QStringLiteral("presentations/demoRemoved"), true);
}

void Presentations::refresh()
{
    if (!m_watcher.directories().isEmpty()) {
        m_watcher.removePaths(m_watcher.directories());
    }
    m_watcher.addPath(m_root);
    m_list.clear();
    QDir root(m_root);
    QStringList folders = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QCollator collator;
    collator.setNumericMode(true);
    std::sort(folders.begin(), folders.end(), collator);
    for (const QString &folder : folders) {
        const QDir dir(root.filePath(folder));
        m_watcher.addPath(dir.absolutePath());
        const QStringList images = sortedImages(dir);
        if (images.isEmpty()) {
            continue;
        }
        const QJsonObject info = readInfo(dir);
        QVariantMap item;
        item.insert(QStringLiteral("folder"), folder);
        item.insert(QStringLiteral("title"), info.value(QStringLiteral("title")).toString(folder));
        item.insert(QStringLiteral("count"), images.size());
        item.insert(QStringLiteral("cover"), QUrl::fromLocalFile(dir.filePath(images.first())).toString());
        item.insert(QStringLiteral("interval"), qBound(3, info.value(QStringLiteral("interval")).toInt(10), 300));
        // Tekst op de dia uit het weekmenu ("Diner van de dag")
        item.insert(QStringLiteral("menu"), info.value(QStringLiteral("menu")).toBool(false));
        m_list.append(item);
        m_watcher.addPath(dir.absolutePath());
    }
    qInfo() << "Presentations:" << m_list.size();
    Q_EMIT changed();
}

QVariantList Presentations::slides(const QString &folder) const
{
    QVariantList out;
    if (folder.contains(QLatin1Char('/')) || folder.contains(QLatin1Char('\\')) || folder.startsWith(QLatin1Char('.'))) {
        return out;
    }
    const QDir dir(m_root + QLatin1Char('/') + folder);
    const QJsonObject captions = readInfo(dir).value(QStringLiteral("captions")).toObject();
    for (const QString &file : sortedImages(dir)) {
        const QJsonObject c = captions.value(file).toObject();
        out.append(QVariantMap{
            {QStringLiteral("source"), QUrl::fromLocalFile(dir.filePath(file)).toString()},
            {QStringLiteral("title"), c.value(QStringLiteral("title")).toString()},
            {QStringLiteral("subtitle"), c.value(QStringLiteral("subtitle")).toString()},
            {QStringLiteral("badge"), c.value(QStringLiteral("badge")).toString()},
        });
    }
    return out;
}

// ---------------------------------------------------------------- beheer

QString Presentations::cleanName(const QString &name)
{
    // Mapnaam: geen padtekens of rare tekens; maximaal 60 tekens
    QString n = name.trimmed();
    n.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|\\x00-\\x1f]")), QStringLiteral(" "));
    n = n.simplified().left(60);
    while (n.startsWith(QLatin1Char('.'))) {
        n.remove(0, 1);
    }
    return n.trimmed();
}

QString Presentations::originalFile(const QString &name) const
{
    const QString clean = cleanName(name);
    if (clean.isEmpty()) {
        return {};
    }
    const QDir dir(m_root + QLatin1Char('/') + clean + QStringLiteral("/origineel"));
    const QStringList files = dir.entryList(QDir::Files);
    return files.isEmpty() ? QString() : dir.filePath(files.first());
}

QByteArray Presentations::toJson() const
{
    QJsonArray out;
    const QDir root(m_root);
    QStringList folders = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QCollator collator;
    collator.setNumericMode(true);
    std::sort(folders.begin(), folders.end(), collator);
    for (const QString &folder : folders) {
        const QDir dir(root.filePath(folder));
        const QString original = originalFile(folder);
        out.append(QJsonObject{
            {QStringLiteral("name"), folder},
            {QStringLiteral("title"), readInfo(dir).value(QStringLiteral("title")).toString(folder)},
            {QStringLiteral("slides"), sortedImages(dir).size()},
            {QStringLiteral("original"), original.isEmpty() ? QString() : QFileInfo(original).fileName()},
            {QStringLiteral("status"), m_status.value(folder)},
        });
    }
    return QJsonDocument(QJsonObject{{QStringLiteral("items"), out}}).toJson(QJsonDocument::Compact);
}

bool Presentations::remove(const QString &name)
{
    const QString clean = cleanName(name);
    if (clean.isEmpty() || clean != name) {
        return false;
    }
    const bool ok = QDir(m_root + QLatin1Char('/') + clean).removeRecursively();
    qInfo() << "Presentations: removed" << clean << ok;
    refresh();
    return ok;
}

bool Presentations::addUpload(const QString &name, const QString &fileName, const QString &tempPath, QString *error)
{
    const QString clean = cleanName(name);
    const QString file = QFileInfo(fileName).fileName().replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_")).left(120);
    if (clean.isEmpty() || file.isEmpty()) {
        *error = QStringLiteral("Geef de presentatie een naam.");
        return false;
    }
    const QString ext = QFileInfo(file).suffix().toLower();
    const bool document = ext == QLatin1String("pptx") || ext == QLatin1String("ppt") || ext == QLatin1String("pps") || ext == QLatin1String("ppsx")
                          || ext == QLatin1String("odp") || ext == QLatin1String("pdf");
    if (!document && !imageFilters.contains(QStringLiteral("*.") + ext)) {
        *error = QStringLiteral("Dit soort bestand kan niet: gebruik PowerPoint (.pptx), PDF of een foto (.jpg, .png).");
        return false;
    }
    const QString dirPath = m_root + QLatin1Char('/') + clean;
    QDir().mkpath(dirPath);
    QDir dir(dirPath);

    if (document) {
        if (m_status.value(clean) == QLatin1String("bezig")) {
            *error = QStringLiteral("Deze presentatie wordt nog omgezet. Even geduld.");
            return false;
        }
        // Origineel bewaren (vorige versie vervangen), zodat het later aangepast en opnieuw geüpload kan worden
        QDir(dir.filePath(QStringLiteral("origineel"))).removeRecursively();
        dir.mkpath(QStringLiteral("origineel"));
        const QString target = dir.filePath(QStringLiteral("origineel/") + file);
        if (!QFile::rename(tempPath, target) && !QFile::copy(tempPath, target)) {
            *error = QStringLiteral("Opslaan mislukt.");
            return false;
        }
        if (!QFile::exists(dir.filePath(QStringLiteral("info.json")))) {
            QFile info(dir.filePath(QStringLiteral("info.json")));
            if (info.open(QIODevice::WriteOnly)) {
                info.write(QJsonDocument(QJsonObject{{QStringLiteral("title"), clean}, {QStringLiteral("interval"), 10}}).toJson());
            }
        }
        qInfo() << "Presentations: original saved" << clean << file;
        convert(clean);
        return true;
    }
    if (imageFilters.contains(QStringLiteral("*.") + ext)) {
        if (QImage(tempPath).isNull()) {
            *error = QStringLiteral("Dit plaatje kan niet gelezen worden.");
            return false;
        }
        const int next = sortedImages(dir).size() + 1;
        const QString target = dir.filePath(QStringLiteral("foto-%1.%2").arg(next, 3, 10, QLatin1Char('0')).arg(ext));
        if (!QFile::rename(tempPath, target) && !QFile::copy(tempPath, target)) {
            *error = QStringLiteral("Opslaan mislukt.");
            return false;
        }
        qInfo() << "Presentations: image added" << clean << target;
        refresh();
        return true;
    }
    *error = QStringLiteral("Dit soort bestand kan niet: gebruik PowerPoint (.pptx), PDF of een foto (.jpg, .png).");
    return false;
}

void Presentations::convert(const QString &name)
{
    const QString source = originalFile(name);
    if (source.isEmpty()) {
        return;
    }
    m_status.insert(name, QStringLiteral("bezig"));
    Q_EMIT changed();
    const QString outDir = m_root + QLatin1Char('/') + name + QStringLiteral("/.omzetten");
    QDir(outDir).removeRecursively();
    QDir().mkpath(outDir);

    if (source.endsWith(QLatin1String(".pdf"), Qt::CaseInsensitive)) {
        QString error;
        finishConversion(name, outDir, pdfToImages(source, outDir, &error) ? QString() : error);
        return;
    }

#ifdef Q_OS_WIN
    // 1. PowerPoint zelf (mooiste resultaat, ook animaties-eindbeeld en lettertypes)
    const QString script = QDir::tempPath() + QStringLiteral("/ppkiosk-ppt-export.ps1");
    QFile::remove(script);
    QFile::copy(QStringLiteral(":/scripts/ppt-export.ps1"), script);
    auto *proc = new QProcess(this);
    proc->setProcessChannelMode(QProcess::MergedChannels);
    connect(proc, &QProcess::finished, this, [this, proc, name, source, outDir](int code) {
        const QString output = QString::fromLocal8Bit(proc->readAll()).trimmed();
        proc->deleteLater();
        qInfo().noquote() << "Presentations: PowerPoint export" << name << "exit" << code << output;
        if (code == 0) {
            finishConversion(name, outDir, QString());
        } else {
            convertWithLibreOffice(name, source);   // 2. LibreOffice als PowerPoint er niet is
        }
    });
    proc->start(QStringLiteral("powershell.exe"),
                {QStringLiteral("-NoProfile"), QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"), QStringLiteral("-File"),
                 QDir::toNativeSeparators(script), QStringLiteral("-Source"), QDir::toNativeSeparators(source),
                 QStringLiteral("-OutDir"), QDir::toNativeSeparators(outDir)});
#else
    convertWithLibreOffice(name, source);
#endif
}

void Presentations::convertWithLibreOffice(const QString &name, const QString &source)
{
    const QString outDir = m_root + QLatin1Char('/') + name + QStringLiteral("/.omzetten");
    QStringList candidates;
    for (const QString &base : {qEnvironmentVariable("ProgramFiles"), qEnvironmentVariable("ProgramFiles(x86)")}) {
        if (!base.isEmpty()) {
            candidates << base + QStringLiteral("/LibreOffice/program/soffice.exe");
        }
    }
    const QString inPath = QStandardPaths::findExecutable(QStringLiteral("soffice"));
    if (!inPath.isEmpty()) {
        candidates << inPath;
    }
    QString soffice;
    for (const QString &c : std::as_const(candidates)) {
        if (QFile::exists(c)) {
            soffice = c;
            break;
        }
    }
    if (soffice.isEmpty()) {
        finishConversion(name, outDir, QStringLiteral("Omzetten lukt niet: installeer PowerPoint of LibreOffice op de kiosk-pc, of upload de presentatie als PDF."));
        return;
    }
    auto *proc = new QProcess(this);
    proc->setProcessChannelMode(QProcess::MergedChannels);
    connect(proc, &QProcess::finished, this, [this, proc, name, source, outDir](int code) {
        proc->deleteLater();
        const QString pdf = outDir + QLatin1Char('/') + QFileInfo(source).completeBaseName() + QStringLiteral(".pdf");
        qInfo() << "Presentations: LibreOffice exit" << code << QFile::exists(pdf);
        QString error;
        if (code != 0 || !QFile::exists(pdf)) {
            error = QStringLiteral("Omzetten met LibreOffice mislukt.");
        } else if (pdfToImages(pdf, outDir, &error)) {
            QFile::remove(pdf);
        }
        finishConversion(name, outDir, error);
    });
    proc->start(soffice, {QStringLiteral("--headless"), QStringLiteral("--norestore"), QStringLiteral("--convert-to"), QStringLiteral("pdf"),
                          QStringLiteral("--outdir"), QDir::toNativeSeparators(outDir), QDir::toNativeSeparators(source)});
}

bool Presentations::pdfToImages(const QString &pdf, const QString &outDir, QString *error)
{
    QPdfDocument doc;
    if (doc.load(pdf) != QPdfDocument::Error::None || doc.pageCount() <= 0) {
        *error = QStringLiteral("PDF kan niet gelezen worden.");
        return false;
    }
    for (int i = 0; i < doc.pageCount() && i < 300; ++i) {
        const QSizeF pt = doc.pagePointSize(i);
        // Passend in 1920x1080, scherp genoeg voor een Full HD-scherm
        const double scale = std::min(1920.0 / pt.width(), 1080.0 / pt.height());
        const QSize size(qRound(pt.width() * scale), qRound(pt.height() * scale));
        QImage page = doc.render(i, size);
        QImage canvas(1920, 1080, QImage::Format_RGB32);
        canvas.fill(Qt::black);
        QPainter p(&canvas);
        p.drawImage((1920 - size.width()) / 2, (1080 - size.height()) / 2, page);
        p.end();
        canvas.save(outDir + QStringLiteral("/dia-%1.png").arg(i + 1, 3, 10, QLatin1Char('0')));
    }
    return true;
}

void Presentations::finishConversion(const QString &name, const QString &outDir, const QString &message)
{
    QDir out(outDir);
    QStringList made = out.entryList(imageFilters, QDir::Files);
    QCollator collator;
    collator.setNumericMode(true);
    std::sort(made.begin(), made.end(), collator);   // Dia2 voor Dia10 (PowerPoint noemt ze Slide1.PNG / Dia1.PNG)
    const bool ok = message.isEmpty() && !made.isEmpty();
    if (ok) {
        // Oude dia's weg, nieuwe erin (losse foto's blijven staan)
        QDir dir(m_root + QLatin1Char('/') + name);
        for (const QString &old : dir.entryList({QStringLiteral("dia-*")}, QDir::Files)) {
            dir.remove(old);
        }
        for (int i = 0; i < made.size(); ++i) {
            QFile::rename(out.filePath(made.at(i)),
                          dir.filePath(QStringLiteral("dia-%1.%2").arg(i + 1, 3, 10, QLatin1Char('0')).arg(QFileInfo(made.at(i)).suffix().toLower())));
        }
        m_status.remove(name);
    } else {
        m_status.insert(name, message.isEmpty() ? QStringLiteral("Omzetten gaf geen dia's.") : message);
    }
    out.removeRecursively();
    qInfo() << "Presentations: conversion" << name << (ok ? "ok" : "failed") << made.size() << message;
    Q_EMIT conversionFinished(name, ok, message);
    refresh();
}
