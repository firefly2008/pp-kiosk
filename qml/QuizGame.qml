// Quiz van de dag: elke dag 5 andere, makkelijke vragen met 3 antwoorden.
// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Layouts

Item {
    id: quiz
    required property var g          // GamesScreen: maten en kleuren
    signal won()
    property var questions: []
    property int index: 0
    property int score: 0
    property int chosen: -1
    readonly property var q: questions.length ? questions[Math.min(index, questions.length - 1)] : null

    // [vraag, goed antwoord, fout, fout]
    readonly property var bank: [
        ["Hoeveel dagen heeft een week?", "7", "5", "10"],
        ["Welke kleur heeft een rijpe banaan?", "Geel", "Paars", "Blauw"],
        ["Wat is de hoofdstad van Nederland?", "Amsterdam", "Rotterdam", "Utrecht"],
        ["Hoeveel poten heeft een spin?", "8", "6", "10"],
        ["Welk dier zegt \"boe\"?", "De koe", "Het schaap", "De eend"],
        ["In welk seizoen vallen de bladeren van de bomen?", "Herfst", "Lente", "Zomer"],
        ["Hoeveel minuten zitten er in een uur?", "60", "30", "100"],
        ["Welke kleur krijg je als je blauw en geel mengt?", "Groen", "Rood", "Paars"],
        ["Wie brengt op 5 december cadeautjes?", "Sinterklaas", "De paashaas", "De kerstman"],
        ["Welke dag komt na vrijdag?", "Zaterdag", "Donderdag", "Maandag"],
        ["Hoeveel provincies heeft Nederland?", "12", "10", "15"],
        ["Op welke dag is Koningsdag (als het geen zondag is)?", "27 april", "5 mei", "30 juni"],
        ["Welk instrument heeft zwarte en witte toetsen?", "Piano", "Gitaar", "Trommel"],
        ["Welke planeet staat het dichtst bij de zon?", "Mercurius", "Mars", "Jupiter"],
        ["Welke kleuren heeft de Nederlandse vlag?", "Rood, wit en blauw", "Rood, geel en zwart", "Groen, wit en rood"],
        ["Welk dier legt eieren?", "De kip", "De koe", "De hond"],
        ["Hoeveel maanden heeft een jaar?", "12", "10", "13"],
        ["In welke stad staat de Domtoren?", "Utrecht", "Zwolle", "Breda"],
        ["Hoe heet de zee tussen Nederland en Engeland?", "De Noordzee", "De Middellandse Zee", "De Oostzee"],
        ["Welk fruit is rood en heeft pitjes aan de buitenkant?", "Aardbei", "Kers", "Appel"],
        ["Waar maak je patat van?", "Aardappelen", "Wortels", "Bloem"],
        ["Wat meet je met een thermometer?", "Temperatuur", "Gewicht", "Lengte"],
        ["Hoeveel hoeken heeft een driehoek?", "3", "4", "5"],
        ["Welk dier heeft een heel lange nek?", "De giraffe", "De olifant", "De leeuw"],
        ["Hoe noem je bevroren water?", "IJs", "Stoom", "Dauw"],
        ["Wie schilderde De Nachtwacht?", "Rembrandt", "Vincent van Gogh", "Piet Mondriaan"],
        ["Wie schilderde De Zonnebloemen?", "Vincent van Gogh", "Rembrandt", "Johannes Vermeer"],
        ["Welk feest vieren we op 25 december?", "Kerstmis", "Pasen", "Pinksteren"],
        ["Hoeveel is 5 + 5?", "10", "15", "55"],
        ["Wat eet je traditioneel met oud en nieuw?", "Oliebollen", "Pepernoten", "Paaseieren"],
        ["In welke provincie ligt Maastricht?", "Limburg", "Brabant", "Zeeland"],
        ["Welk eiland is een Waddeneiland?", "Texel", "Mallorca", "Kreta"],
        ["Hoe heet de schaatstocht langs elf Friese steden?", "Elfstedentocht", "Alpe d'HuZes", "Vierdaagse"],
        ["Welk dier geeft wol?", "Het schaap", "Het paard", "De geit"],
        ["Hoeveel uur zit er in een dag?", "24", "12", "48"],
        ["Welke vogel is het teken van vrede?", "De duif", "De kraai", "De meeuw"],
        ["In welke stad staat de Eiffeltoren?", "Parijs", "Londen", "Berlijn"],
        ["Wat is de hoofdstad van België?", "Brussel", "Antwerpen", "Gent"],
        ["In welk seizoen is het meestal het warmst?", "Zomer", "Winter", "Herfst"],
        ["Wat maakt een bij?", "Honing", "Melk", "Zijde"],
        ["Welke oranje groente eet een konijn graag?", "Wortel", "Komkommer", "Prei"],
        ["Hoe heet het koekje met stroop tussen twee wafels?", "Stroopwafel", "Speculaas", "Bitterkoekje"],
        ["Hoeveel spelers van één team staan er bij voetbal op het veld?", "11", "9", "15"],
        ["Welke kleur dragen de spelers van het Nederlands elftal?", "Oranje", "Blauw", "Groen"],
        ["In welke maand begint de lente?", "Maart", "Januari", "Juli"],
        ["Welk dier woont in een hok en zegt \"knor\"?", "Het varken", "De kat", "Het paard"],
        ["Wat heb je nodig om een brief te versturen?", "Een postzegel", "Een sleutel", "Een paraplu"],
        ["Welke kleur heeft een brandweerauto meestal?", "Rood", "Groen", "Wit"],
        ["Hoeveel wielen heeft een fiets?", "2", "3", "4"],
        ["Welke molen staat bekend in Kinderdijk?", "Een windmolen", "Een watertoren", "Een vuurtoren"],
        ["Welk getal komt na negentien?", "Twintig", "Achttien", "Dertig"],
        ["Wat groeit er aan een appelboom?", "Appels", "Peren", "Bananen"],
        ["Waar zwemt een vis?", "In het water", "In de lucht", "In het zand"],
        ["Wat doe je met een paraplu?", "Droog blijven als het regent", "Soep eten", "Fietsen"],
        ["Welke dag is het eerste van het weekend?", "Zaterdag", "Woensdag", "Maandag"]
    ]

    // Vaste volgorde per dag (iedereen krijgt vandaag dezelfde vragen)
    function rng(seed) { return function() { seed = (seed * 1103515245 + 12345) % 2147483648; return seed / 2147483648 } }
    function start() {
        const d = new Date()
        const r = rng(d.getFullYear() * 1000 + d.getMonth() * 40 + d.getDate())
        const pool = bank.slice()
        const out = []
        for (let k = 0; k < 5; ++k) {
            const q = pool.splice(Math.floor(r() * pool.length), 1)[0]
            const answers = [q[1], q[2], q[3]]
            for (let i = answers.length - 1; i > 0; i--) { const j = Math.floor(r() * (i + 1)); [answers[i], answers[j]] = [answers[j], answers[i]] }
            out.push({ text: q[0], answers: answers, good: answers.indexOf(q[1]) })
        }
        questions = out; index = 0; score = 0; chosen = -1
        say()
        console.info("Game started: quiz 5 questions")
    }
    function say() { if (agenda.settings.readAloud !== false && q) speaker.say(q.text + " " + q.answers.join(", of ")) }
    function answer(i) {
        if (chosen >= 0) return
        chosen = i
        if (i === q.good) score++
        nextTimer.restart()
    }
    Timer {
        id: nextTimer
        interval: 2200
        onTriggered: {
            if (quiz.index >= quiz.questions.length - 1) { console.info("Game won: quiz " + quiz.score + "/5"); quiz.won(); return }
            quiz.index++; quiz.chosen = -1; quiz.say()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 28 * quiz.g.s
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "Vraag " + (quiz.index + 1) + " van " + quiz.questions.length + "   ·   goed: " + quiz.score
            font.pixelSize: 40 * quiz.g.s; color: quiz.g.soft
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 260 * quiz.g.s
            radius: 36 * quiz.g.s
            color: quiz.g.cardBg; border.color: quiz.g.lineColor; border.width: 2
            Text {
                anchors { fill: parent; margins: 40 * quiz.g.s }
                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; wrapMode: Text.WordWrap
                text: quiz.q ? quiz.q.text : ""
                font.pixelSize: 64 * quiz.g.s; font.bold: true; color: quiz.g.ink
                fontSizeMode: Text.Fit; minimumPixelSize: 36 * quiz.g.s
            }
        }
        Repeater {
            model: quiz.q ? quiz.q.answers : []
            delegate: Rectangle {
                required property var modelData
                required property int index
                readonly property bool isGood: quiz.chosen >= 0 && index === quiz.q.good
                readonly property bool isWrong: quiz.chosen === index && index !== quiz.q.good
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: height / 2
                color: isGood ? "#2E8B57" : isWrong ? "#C0392B" : ansTap.pressed ? quiz.g.pressBg : quiz.g.cardBg
                border.color: isGood ? "#8BE9C0" : quiz.g.lineColor; border.width: isGood ? 4 : 2
                RowLayout {
                    anchors { fill: parent; leftMargin: 50 * quiz.g.s; rightMargin: 50 * quiz.g.s }
                    Text { text: ["A", "B", "C"][index]; font.pixelSize: 52 * quiz.g.s; font.bold: true; color: isGood || isWrong ? "#FFFFFF" : quiz.g.soft }
                    Text {
                        Layout.fillWidth: true; Layout.leftMargin: 30 * quiz.g.s
                        text: modelData; font.pixelSize: 52 * quiz.g.s; font.bold: true
                        color: isGood || isWrong ? "#FFFFFF" : quiz.g.ink; elide: Text.ElideRight
                    }
                    Text { text: isGood ? "✓" : isWrong ? "✗" : ""; font.pixelSize: 60 * quiz.g.s; font.bold: true; color: "#FFFFFF" }
                }
                TapHandler { id: ansTap; gesturePolicy: TapHandler.ReleaseWithinBounds; onTapped: quiz.answer(index) }
            }
        }
    }
}
