// SPDX-License-Identifier: GPL-2.0-or-later
#include "menuboard.h"
#include "agenda.h"

#include <QCryptographicHash>
#include <QDate>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QSaveFile>
#include <QtMath>

namespace {
const QColor red(0xD3, 0x21, 0x1B);
const QColor darkRed(0x8A, 0x10, 0x0C);
const QColor yellow(0xFF, 0xBC, 0x0D);
const QColor cream(0xFF, 0xE9, 0xC4);

QFont boardFont(int pixelSize, int weight)
{
    QFont f;
    f.setFamilies({QStringLiteral("Segoe UI"), QStringLiteral("Arial"), QStringLiteral("DejaVu Sans")});
    f.setPixelSize(pixelSize);
    f.setWeight(QFont::Weight(weight));
    return f;
}

// Tekst zo groot mogelijk (tussen max en min) in het vak laten passen
int fitText(QPainter &p, const QRect &box, const QString &text, int maxSize, int minSize, int weight)
{
    int size = maxSize;
    for (; size > minSize; size -= 4) {
        p.setFont(boardFont(size, weight));
        const QRect need = p.fontMetrics().boundingRect(box, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, text);
        if (need.height() <= box.height() && need.width() <= box.width()) {
            break;
        }
    }
    p.setFont(boardFont(size, weight));
    return p.fontMetrics().boundingRect(box, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, text).height();
}

QPainterPath starburst(QPointF c, double outer, double inner, int points)
{
    QPainterPath path;
    for (int i = 0; i < points * 2; ++i) {
        const double r = (i % 2 == 0) ? outer : inner;
        const double a = M_PI * i / points - M_PI / 2;
        const QPointF pt(c.x() + r * qCos(a), c.y() + r * qSin(a));
        if (i == 0) {
            path.moveTo(pt);
        } else {
            path.lineTo(pt);
        }
    }
    path.closeSubpath();
    return path;
}
} // namespace

MenuBoard::MenuBoard(Agenda *agenda, const QString &presentationsRoot, QObject *parent)
    : QObject(parent)
    , m_agenda(agenda)
    , m_dir(presentationsRoot + QLatin1Char('/') + folderName())
{
    connect(agenda, &Agenda::changed, this, [this] { update(false); });
    update(true);
}

QString MenuBoard::photoFor(const QString &dish, const QString &note)
{
    // Volgorde telt: "Vis met friet" is vis, "Hutspot met gehaktbal" is stamppot
    static const QList<QPair<QString, QStringList>> table = {
        {QStringLiteral("soep"), {QStringLiteral("soep"), QStringLiteral("bouillon")}},
        {QStringLiteral("pannenkoek"), {QStringLiteral("pannenkoek"), QStringLiteral("poffertje"), QStringLiteral("wentelteefje"), QStringLiteral("flensje")}},
        {QStringLiteral("pizza"), {QStringLiteral("pizza")}},
        {QStringLiteral("burger"), {QStringLiteral("burger")}},
        {QStringLiteral("vis"), {QStringLiteral("vis"), QStringLiteral("kibbeling"), QStringLiteral("zalm"), QStringLiteral("kabeljauw"), QStringLiteral("lekkerbek"),
                                 QStringLiteral("tonijn"), QStringLiteral("garnal"), QStringLiteral("haring"), QStringLiteral("pangasius"), QStringLiteral("koolvis")}},
        {QStringLiteral("friet"), {QStringLiteral("friet"), QStringLiteral("patat"), QStringLiteral("frikandel"), QStringLiteral("kroket"), QStringLiteral("snack"), QStringLiteral("frites")}},
        {QStringLiteral("nasi"), {QStringLiteral("nasi"), QStringLiteral("bami"), QStringLiteral("saté"), QStringLiteral("sate"), QStringLiteral("babi"), QStringLiteral("indisch"),
                                  QStringLiteral("chinees"), QStringLiteral("wok"), QStringLiteral("noedel"), QStringLiteral("loempia"), QStringLiteral("mihoen")}},
        {QStringLiteral("pasta"), {QStringLiteral("macaroni"), QStringLiteral("spaghetti"), QStringLiteral("pasta"), QStringLiteral("lasagne"), QStringLiteral("penne"),
                                   QStringLiteral("tagliatelle"), QStringLiteral("ravioli"), QStringLiteral("tortellini"), QStringLiteral("ovenschotel")}},
        {QStringLiteral("stamppot"), {QStringLiteral("stamppot"), QStringLiteral("hutspot"), QStringLiteral("zuurkool"), QStringLiteral("boerenkool"), QStringLiteral("andijvie"),
                                      QStringLiteral("puree"), QStringLiteral("stoof")}},
        {QStringLiteral("salade"), {QStringLiteral("salade"), QStringLiteral("sla ")}},
        {QStringLiteral("kip-rijst"), {QStringLiteral("kip"), QStringLiteral("rijst"), QStringLiteral("curry")}},
        {QStringLiteral("aardappel-vlees"), {QStringLiteral("aardappel"), QStringLiteral("gehakt"), QStringLiteral("schnitzel"), QStringLiteral("biefstuk"),
                                             QStringLiteral("karbonade"), QStringLiteral("slavink"), QStringLiteral("worst"), QStringLiteral("vlees")}},
    };
    // Eerst alleen het gerecht, dan ook de toelichting
    for (const QString &text : {dish.toLower() + QLatin1Char(' '), (dish + QLatin1Char(' ') + note).toLower() + QLatin1Char(' ')}) {
        for (const auto &row : table) {
            for (const QString &word : row.second) {
                if (text.contains(word)) {
                    return row.first;
                }
            }
        }
    }
    return QStringLiteral("aardappel-vlees");
}

QImage MenuBoard::render(const QString &dayLabel, const QString &dateText, const QString &dish, const QString &note,
                         const QString &time, const QImage &photo)
{
    QImage img(1920, 1080, QImage::Format_RGB32);
    QPainter p(&img);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);

    // Achtergrond: warm rood met schuine banen, zoals een menubord
    QLinearGradient bg(0, 0, 1920, 1080);
    bg.setColorAt(0, red);
    bg.setColorAt(1, darkRed);
    p.fillRect(img.rect(), bg);
    p.save();
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 14));
    for (int x = -1200; x < 1920; x += 240) {
        QPolygonF band;
        band << QPointF(x, 1080) << QPointF(x + 120, 1080) << QPointF(x + 120 + 1080, 0) << QPointF(x + 1080, 0);
        p.drawPolygon(band);
    }
    p.restore();
    // Gele baan onderaan
    p.fillRect(QRect(0, 1040, 1920, 40), yellow);

    // Foto in een rond bord met gele en witte rand
    const QPointF c(560, 520);
    const double r = 410;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 70));
    p.drawEllipse(c + QPointF(14, 22), r + 26, r + 26);
    p.setBrush(yellow);
    p.drawEllipse(c, r + 26, r + 26);
    p.setBrush(Qt::white);
    p.drawEllipse(c, r + 8, r + 8);
    if (!photo.isNull()) {
        const QImage square = photo.scaled(int(2 * r), int(2 * r), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        QPainterPath clip;
        clip.addEllipse(c, r, r);
        p.save();
        p.setClipPath(clip);
        p.drawImage(QPointF(c.x() - square.width() / 2.0, c.y() - square.height() / 2.0), square);
        p.restore();
    }

    // Rechts: dag, gerecht, toelichting
    const int left = 1080;
    const int width = 1920 - left - 80;
    p.setFont(boardFont(58, QFont::Black));
    const QString label = dayLabel.toUpper();
    const int pillW = p.fontMetrics().horizontalAdvance(label) + 80;
    p.setBrush(yellow);
    p.drawRoundedRect(QRectF(left, 90, pillW, 96), 48, 48);
    p.setPen(darkRed);
    p.drawText(QRectF(left, 90, pillW, 96), Qt::AlignCenter, label);

    p.setPen(cream);
    p.setFont(boardFont(40, QFont::DemiBold));
    p.drawText(QRect(left + 4, 200, width, 56), Qt::AlignLeft | Qt::AlignVCenter, dateText);

    const QRect dishBox(left, 290, width, 360);
    p.setPen(Qt::white);
    const int used = fitText(p, dishBox, dish.toUpper(), 124, 60, QFont::Black);
    // Schaduw onder de titel
    p.save();
    p.setPen(QColor(0, 0, 0, 90));
    p.drawText(dishBox.translated(4, 5), Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, dish.toUpper());
    p.restore();
    p.drawText(dishBox, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, dish.toUpper());

    if (!note.isEmpty()) {
        const QRect noteBox(left + 4, dishBox.top() + used + 20, width, 150);
        p.setPen(cream);
        fitText(p, noteBox, note, 52, 34, QFont::DemiBold);
        p.drawText(noteBox, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, note);
    }

    // Tijd in een gele ster
    const QPointF sc(1715, 865);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 70));
    p.drawPath(starburst(sc + QPointF(8, 12), 152, 128, 20));
    p.setBrush(yellow);
    p.drawPath(starburst(sc, 152, 128, 20));
    p.setPen(darkRed);
    p.setFont(boardFont(68, QFont::Black));
    p.drawText(QRectF(sc.x() - 150, sc.y() - 72, 300, 96), Qt::AlignCenter, time);
    p.setFont(boardFont(44, QFont::Bold));
    p.drawText(QRectF(sc.x() - 150, sc.y() + 14, 300, 60), Qt::AlignCenter, QStringLiteral("uur"));

    p.setPen(Qt::white);
    p.setFont(boardFont(54, QFont::Black));
    p.drawText(QRect(left + 4, 910, 460, 90), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Eet smakelijk!"));
    p.end();
    return img;
}

QImage MenuBoard::renderGlass(const QString &dayLabel, const QString &dateText, const QString &dish, const QString &note,
                              const QString &time, const QImage &photo)
{
    QImage img(1920, 1080, QImage::Format_RGB32);
    QPainter p(&img);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
    const QColor soft(0xC3, 0xCA, 0xE0);
    const QColor mint(0x8B, 0xE9, 0xC0);

    // Donkere achtergrond met zachte, lichtgevende kleurvlekken
    p.fillRect(img.rect(), QColor(0x0A, 0x0F, 0x1E));
    const struct { const char *name; int x, y, size; double opacity; } blobs[] = {
        {"violet", 220, 180, 1300, 0.8}, {"blauw", 1700, 260, 1200, 0.75}, {"turquoise", 520, 1050, 1200, 0.7}, {"roze", 1820, 1000, 1100, 0.55}};
    for (const auto &b : blobs) {
        const QImage blob = QImage(QStringLiteral(":/glass/%1.png").arg(QLatin1String(b.name)))
                                .scaled(b.size, b.size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        p.setOpacity(b.opacity);
        p.drawImage(b.x - b.size / 2, b.y - b.size / 2, blob);
    }
    p.setOpacity(1);

    // Grote glaskaart
    const QRectF card(80, 80, 1760, 920);
    p.setPen(QPen(QColor(255, 255, 255, 90), 2));
    p.setBrush(QColor(255, 255, 255, 34));
    p.drawRoundedRect(card, 56, 56);
    // Lichte rand bovenaan (lichtbreking)
    p.setPen(QPen(QColor(255, 255, 255, 140), 2));
    p.drawLine(QPointF(card.left() + 70, card.top() + 3), QPointF(card.right() - 70, card.top() + 3));

    // Foto links, afgeronde hoeken
    const QRectF photoBox(130, 130, 820, 820);
    if (!photo.isNull()) {
        const QImage square = photo.scaled(int(photoBox.width()), int(photoBox.height()), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        QPainterPath clip;
        clip.addRoundedRect(photoBox, 40, 40);
        p.save();
        p.setClipPath(clip);
        p.drawImage(QPointF(photoBox.center().x() - square.width() / 2.0, photoBox.center().y() - square.height() / 2.0), square);
        p.restore();
        p.setPen(QPen(QColor(255, 255, 255, 70), 2));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(photoBox, 40, 40);
    }

    // Rechts: dag (pill), datum, gerecht, toelichting
    const int left = 1030;
    const int width = 1920 - left - 150;
    p.setFont(boardFont(46, QFont::Bold));
    const QString label = dayLabel.left(1).toUpper() + dayLabel.mid(1);
    const int pillW = p.fontMetrics().horizontalAdvance(label) + 72;
    p.setPen(QPen(QColor(255, 255, 255, 110), 2));
    p.setBrush(QColor(255, 255, 255, 46));
    p.drawRoundedRect(QRectF(left, 150, pillW, 84), 42, 42);
    p.setPen(Qt::white);
    p.drawText(QRectF(left, 150, pillW, 84), Qt::AlignCenter, label);

    p.setPen(soft);
    p.setFont(boardFont(38, QFont::Medium));
    p.drawText(QRect(left + 6, 250, width, 56), Qt::AlignLeft | Qt::AlignVCenter, dateText);

    const QRect dishBox(left, 330, width, 290);
    p.setPen(Qt::white);
    const int used = fitText(p, dishBox, dish, 116, 58, QFont::Bold);
    p.drawText(dishBox, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, dish);
    if (!note.isEmpty()) {
        const QRect noteBox(left + 4, dishBox.top() + used + 16, width, qMax(60, 740 - (dishBox.top() + used + 16)));
        p.setPen(soft);
        fitText(p, noteBox, note, 46, 30, QFont::Medium);
        p.drawText(noteBox, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, note);
    }

    // Tijd: gloeiende pill
    const QRectF timeBox(left, 760, 420, 120);
    QLinearGradient tg(timeBox.topLeft(), timeBox.bottomRight());
    tg.setColorAt(0, QColor(139, 92, 246, 230));
    tg.setColorAt(1, QColor(59, 130, 246, 230));
    p.setPen(QPen(QColor(255, 255, 255, 120), 2));
    p.setBrush(tg);
    p.drawRoundedRect(timeBox, 60, 60);
    p.setPen(Qt::white);
    p.setFont(boardFont(60, QFont::Bold));
    p.drawText(timeBox, Qt::AlignCenter, time + QStringLiteral(" uur"));

    p.setPen(mint);
    p.setFont(boardFont(46, QFont::Bold));
    p.drawText(QRect(left + 6, 900, width, 70), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Eet smakelijk!"));
    p.end();
    return img;
}

void MenuBoard::update(bool force)
{
    const QDate today = QDate::currentDate();
    const QJsonArray menu = m_agenda->weekMenu();
    const QString time = m_agenda->settings().value(QStringLiteral("dinnerTime")).toString();
    const QVariantMap cfg = m_agenda->settings();
    const bool glass = cfg.value(QStringLiteral("theme")).toString() == QLatin1String("glas") && !cfg.value(QStringLiteral("contrast")).toBool();
    const QString key = today.toString(Qt::ISODate) + time + (glass ? QStringLiteral("glas") : QString()) + QString::fromUtf8(QJsonDocument(menu).toJson(QJsonDocument::Compact));
    // Opnieuw maken als het menu, de tijd of de dag verandert, of als de map is weggehaald
    if (!force && key == m_key && QFileInfo::exists(m_dir + QStringLiteral("/info.json"))) {
        return;
    }
    m_key = key;

    QDir dir(m_dir);
    dir.mkpath(QStringLiteral("."));
    for (const QString &old : dir.entryList({QStringLiteral("*.jpg"), QStringLiteral("*.png")}, QDir::Files)) {
        dir.remove(old);
    }
    const QLocale nl(QLocale::Dutch, QLocale::Netherlands);
    int count = 0;
    for (int i = 0; i < 7; ++i) {
        const QDate day = today.addDays(i);
        const QJsonObject o = menu.at(day.dayOfWeek() - 1).toObject();
        const QString dish = o.value(QStringLiteral("dish")).toString().trimmed();
        if (dish.isEmpty()) {
            continue;
        }
        const QString note = o.value(QStringLiteral("note")).toString().trimmed();
        const QString own = o.value(QStringLiteral("image")).toString();
        QImage photo;
        if (!own.isEmpty()) {
            photo.load(m_agenda->pictoDir() + QLatin1Char('/') + own);
        }
        if (photo.isNull()) {
            photo.load(QStringLiteral(":/food/") + photoFor(dish, note) + QStringLiteral(".jpg"));
        }
        const QString label = i == 0 ? QStringLiteral("Vandaag") : i == 1 ? QStringLiteral("Morgen") : nl.dayName(day.dayOfWeek());
        const QString date = (i < 2 ? nl.dayName(day.dayOfWeek()) + QLatin1Char(' ') : QString()) + nl.toString(day, QStringLiteral("d MMMM"));
        const QImage slide = glass ? renderGlass(label, date, dish, note, time, photo) : render(label, date, dish, note, time, photo);
        slide.save(dir.filePath(QStringLiteral("%1.jpg").arg(++count)), "JPG", 90);
    }
    QSaveFile info(dir.filePath(QStringLiteral("info.json")));
    if (info.open(QIODevice::WriteOnly)) {
        info.write(QJsonDocument(QJsonObject{
                                     {QStringLiteral("title"), folderName()},
                                     {QStringLiteral("interval"), 10},
                                     {QStringLiteral("menu"), false},
                                     {QStringLiteral("auto"), true},
                                 })
                       .toJson());
        info.commit();
    }
    qInfo() << "Menu board:" << count << "slides" << (glass ? "glass" : "");
}
