import xml.etree.ElementTree as ET
exec(open('i18n/tools/en_dict.py').read())
SLOGANS = {
"Grootste zender van Nederland":"Biggest station in the Netherlands","Hits en de Top 40":"Hits and the Top 40","Dance en hits":"Dance and hits",
"Non-stop muziek":"Non-stop music","Hits van toen":"Hits from back then","Hits van de jaren 80, 90 en nu":"Hits from the 80s, 90s and today",
"Rock en classics":"Rock and classics","Nieuws en sport":"News and sport","Nieuwe muziek":"New music","Klassieke muziek":"Classical music",
"Hits van de jaren 60, 70 en 80":"Hits from the 60s, 70s and 80s","Urban en hiphop":"Urban and hip-hop","Alleen Nederlandse muziek":"Dutch music only",
"Soul, funk en jazz":"Soul, funk and jazz","Dance":"Dance","Alternatieve rock":"Alternative rock","Classic rock":"Classic rock",
"Zakelijk nieuws":"Business news","Hits van toen en nu":"Hits from then and now","Christelijke muziek":"Christian music",
"Hits uit Amsterdam":"Hits from Amsterdam","Gezellige muziek":"Feel-good music","Easy listening":"Easy listening","Klassiek":"Classical",
"Motorsport":"Motorsport","Oude muziek":"Early music","Hits zonder praatjes":"Hits without chatter","Foute hits":"Guilty-pleasure hits",
"Liefdesliedjes":"Love songs","Hitzone":"Hitzone","De Top 4000":"The Top 4000","Hits uit de jaren 70 en 80":"Hits from the 70s and 80s",
"Dance zonder praatjes":"Dance without chatter","Rotterdam en omgeving":"Rotterdam and surroundings","Regionale omroep":"Regional broadcaster",
}
DYN = {"Favorieten":"Favourites","Alle zenders":"All channels","Regionale tv":"Regional TV","Spatie":"Space","Wissen":"Clear","OK":"OK"}
tree=ET.parse('i18n/kijkluister_en.ts'); root=tree.getroot()
root.set('language','en_GB'); root.set('sourcelanguage','nl_NL')
def add(ctx, src, tr):
    m=ET.SubElement(ctx,'message'); ET.SubElement(m,'source').text=src; t=ET.SubElement(m,'translation'); t.text=tr
for ctx in root.findall('context'):
    name=ctx.find('name').text
    have={ (m.find('source').text or '') for m in ctx.findall('message')}
    if name in ('IptvScreen','GuideGrid','OnScreenKeyboard','Main','EpgPanel'):
        for k,v in DYN.items():
            if k not in have: add(ctx,k,v)
c=ET.SubElement(root,'context'); ET.SubElement(c,'name').text='RadioSlogans'
for k,v in SLOGANS.items(): add(c,k,v)
missing=0
for m in root.iter('message'):
    s=m.find('source').text or ''
    t=m.find('translation')
    if t is None: t=ET.SubElement(m,'translation')
    if s in T: t.text=T[s]
    elif s in SLOGANS or s in DYN: t.text=SLOGANS.get(s,DYN.get(s))
    elif s=='': t.text=s
    else: missing+=1; print('MISSING',repr(s))
    if 'type' in t.attrib: del t.attrib['type']
tree.write('i18n/kijkluister_en.ts',encoding='utf-8',xml_declaration=True)
print('missing',missing)
