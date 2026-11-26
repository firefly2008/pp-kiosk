# Spraakbediening: Nederlandse spraakherkenning van Windows (offline, met een vaste lijst opdrachten).
# Start: powershell -File listen.ps1 -Phrases "zet de radio aan|hoe laat is het|..."
# Leest "listen" op stdin en schrijft per keer: HEARD<tab>tekst<tab>zekerheid, NOTHING<tab>reden of ERROR<tab>melding.
# SPDX-License-Identifier: GPL-2.0-or-later
param([string]$Phrases)
$ErrorActionPreference = 'Stop'
function Say([string]$s) { [Console]::Out.WriteLine($s); [Console]::Out.Flush() }
try {
    Add-Type -AssemblyName System.Runtime.WindowsRuntime
    $null = [Windows.Media.SpeechRecognition.SpeechRecognizer, Windows.Media.SpeechRecognition, ContentType = WindowsRuntime]
    $null = [Windows.Globalization.Language, Windows.Globalization, ContentType = WindowsRuntime]
    $asTask = ([System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object {
        $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1' })[0]
    function Await($op, [Type]$type) {
        $t = $asTask.MakeGenericMethod($type).Invoke($null, @($op))
        $t.Wait(-1) | Out-Null
        $t.Result
    }
    $nl = [Windows.Media.SpeechRecognition.SpeechRecognizer]::SupportedGrammarLanguages |
        Where-Object { $_.LanguageTag -like 'nl*' } | Select-Object -First 1
    if (-not $nl) {
        Say "ERROR`tNederlandse spraakherkenning staat niet op deze pc (Windows-instellingen > Tijd en taal > Taal: Nederlands > Spraak)."
        exit 2
    }
    $rec = [Windows.Media.SpeechRecognition.SpeechRecognizer]::new($nl)
    $list = [System.Collections.Generic.List[string]]::new()
    foreach ($p in $Phrases.Split('|')) { if ($p.Trim()) { $list.Add($p.Trim()) } }
    $rec.Constraints.Add([Windows.Media.SpeechRecognition.SpeechRecognitionListConstraint]::new($list, 'opdrachten'))
    $comp = Await ($rec.CompileConstraintsAsync()) ([Windows.Media.SpeechRecognition.SpeechRecognitionCompilationResult])
    if ($comp.Status -ne 'Success') {
        Say "ERROR`tSpraakherkenning starten lukt niet ($($comp.Status))."
        exit 3
    }
    $rec.Timeouts.InitialSilenceTimeout = [TimeSpan]::FromSeconds(6)
    $rec.Timeouts.EndSilenceTimeout = [TimeSpan]::FromSeconds(1)
    Say "READY`t$($nl.LanguageTag)"
    while ($null -ne ($line = [Console]::In.ReadLine())) {
        if ($line.Trim() -ne 'listen') { continue }
        try {
            $r = Await ($rec.RecognizeAsync()) ([Windows.Media.SpeechRecognition.SpeechRecognitionResult])
            if ($r.Status -eq 'Success' -and $r.Text -and $r.Confidence -ne 'Rejected') {
                Say "HEARD`t$($r.Text)`t$($r.Confidence)"
            } else {
                Say "NOTHING`t$($r.Status)"
            }
        } catch {
            Say "ERROR`t$($_.Exception.Message)"
        }
    }
} catch {
    Say "ERROR`t$($_.Exception.Message)"
    exit 1
}
