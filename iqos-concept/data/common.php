<?php

declare(strict_types=1);

// Conținut comun tuturor paginilor: navigație, footer, avertismente,
// dispozitive, rezerve. Textele și imaginile sunt preluate de pe iqos.ro.

require_once __DIR__ . '/helpers.php';

return [
    'site_name' => 'IQOS Romania',
    'logo' => cdn('/design/themes/kura/media/images/logo/logo.svg'),

    'age_gate' => [
        'intro' => 'Introdu data nașterii pentru a confirma că ești un utilizator adult (peste 18 ani) de produse cu nicotină, rezident în România.',
        'error' => 'Trebuie să ai peste 18 ani pentru a accesa acest website.',
        'disclaimer' => 'Acest website conține informații despre produsele fără fum ale Philip Morris Trading SRL, destinate exclusiv adulților din România care altfel ar continua să folosească alte produse cu nicotină convenționale (i.e. care nu sunt fără fum). Produsele fără fum nu sunt alternative pentru renunțarea la consumul de produse cu nicotină convenționale, și nu au fost create ca facilitatori pentru o astfel de renunțare. Produsele fără fum nu sunt lipsite de riscuri și conțin nicotină, care creează dependență.',
        'important_url' => iqos('/descopera-iqos/important'),
        'months' => ['Ianuarie', 'Februarie', 'Martie', 'Aprilie', 'Mai', 'Iunie', 'Iulie', 'August', 'Septembrie', 'Octombrie', 'Noiembrie', 'Decembrie'],
        'years' => range((int) date('Y'), (int) date('Y') - 100),
    ],

    'health_warning' => 'Acest produs nu este lipsit de riscuri. Eliberează nicotină, care provoacă dependență. IQOS este destinat adulților care altfel ar continua să fumeze sau să folosească produse cu nicotină.',
    'eu_warning' => [
        'source' => 'Directiva Consiliului CE 89/622/CEE:',
        'text' => 'Tutunul dăunează grav sănătății',
    ],
    'sticks_warning' => 'Nu dezasamblați sau înghițiți rezervele SMARTCORE STICKS™. Acestea conțin o parte metalică ascuțită, care poate produce răni serioase în caz de înghițire. Nu lăsați la îndemâna copiilor. Mai multe informații pe iqos.ro.',

    // Navigația globală. Elementele cu 'groups' deschid un meniu flyout.
    'nav' => [
        [
            'label' => 'Shop',
            'groups' => [
                ['title' => 'Cumpără acum', 'big' => true, 'links' => [
                    ['label' => 'IQOS ILUMA', 'url' => iqos('/shop-iluma/disp-iluma')],
                    ['label' => 'TEREA', 'url' => iqos('/shop-iluma/rezerve-iqos/i-terea')],
                    ['label' => 'LEVIA', 'url' => iqos('/shop-iluma/rezerve-iqos/i-levia')],
                    ['label' => 'DELIA', 'url' => iqos('/shop-iluma/rezerve-iqos/i-delia')],
                    ['label' => 'Toate rezervele', 'url' => iqos('/shop-iluma/rezerve-iqos')],
                ]],
                ['title' => 'Mai multe', 'links' => [
                    ['label' => 'Despre rezervele ILUMA', 'url' => iqos('/rezerve-compatibile-cu-iqos-iluma')],
                    ['label' => 'Compară dispozitivele', 'url' => iqos('/despre-iqos/compara-dispozitivele-iqos')],
                    ['label' => 'Abonament Rezerve', 'url' => iqos('/auto-delivery-landing-page')],
                ]],
                ['title' => 'Descoperă alte categorii', 'links' => [
                    ['label' => 'Descoperă ZYN · Pliculețe cu nicotină', 'url' => 'https://www.zyn.com/ro/'],
                    ['label' => 'Descoperă VEEV · Țigaretă electronică', 'url' => 'http://veev-vape.com/ro/'],
                ]],
            ],
        ],
        [
            'label' => 'Despre produse',
            'groups' => [
                ['title' => 'Toate informațiile de care ai nevoie', 'big' => true, 'links' => [
                    ['label' => 'IQOS ILUMA i PRIME', 'url' => local('iluma-i-prime.php')],
                    ['label' => 'Descoperă IQOS', 'url' => iqos('/despre-iqos/descopera-iluma')],
                    ['label' => 'Rezerve compatibile cu IQOS', 'url' => iqos('/rezerve-compatibile-cu-iqos-iluma')],
                    ['label' => 'Beneficiile IQOS', 'url' => iqos('/despre-iqos/beneficii')],
                    ['label' => 'Știința', 'url' => iqos('/despre-nicotina-gudronul-si-tutun-iqos')],
                ]],
                ['title' => 'Descoperă alte categorii', 'links' => [
                    ['label' => 'Descoperă ZYN · Pliculețe cu nicotină', 'url' => 'https://www.zyn.com/ro/'],
                    ['label' => 'Descoperă VEEV · Țigaretă electronică', 'url' => 'http://veev-vape.com/ro/'],
                ]],
            ],
        ],
        [
            'label' => 'Suport',
            'groups' => [
                ['title' => 'Suport', 'big' => true, 'links' => [
                    ['label' => 'Contactează-ne', 'url' => iqos('/contacteaza-iqos')],
                    ['label' => 'Asistență', 'url' => iqos('/myiqos/asistenta')],
                    ['label' => 'Device Care Hub', 'url' => iqos('/myiqos/device-care-hub')],
                    ['label' => 'Diagnoză online', 'url' => iqos('/index.php?dispatch=aqure.journey')],
                    ['label' => 'Garanția dispozitivelor IQOS', 'url' => iqos('/garantie')],
                    ['label' => 'Trade-in', 'url' => iqos('/trade-in')],
                ]],
                ['title' => 'Servicii & Beneficii', 'links' => [
                    ['label' => 'Descoperă Servicii & Beneficii', 'url' => iqos('/myiqos-ro')],
                    ['label' => 'Beneficiază de suport', 'url' => iqos('/myiqos-ro#getsupport')],
                    ['label' => 'Bucură-te de beneficii', 'url' => iqos('/myiqos-ro#enjoy')],
                ]],
            ],
        ],
        ['label' => 'Colaborări', 'url' => iqos('/colaborari-iqos')],
        ['label' => 'IQOS CLUB', 'url' => iqos('/auth-loginform?loyalty=1')],
        ['label' => 'Încearcă', 'url' => iqos('/incearca-iqos')],
        ['label' => 'Magazine', 'url' => iqos('/store/store-locator')],
        ['label' => 'Zero Compromisuri', 'url' => 'http://www.zerocompromisuri.ro'],
    ],
    'account_url' => iqos('/auth-loginform'),
    'cart_url' => iqos('/checkout-cart'),
    'home_url' => local('index.php'),

    // Cele trei dispozitive din „Compară dispozitivele”.
    'devices' => [
        [
            'id' => 'one',
            'name' => 'IQOS ILUMA i ONE',
            'tagline' => 'Design all-in-one',
            'image' => cdn('/images/digital/iluma-4-1/PDP/IQOS_ILUMA-I-ONE_IILUMAIL24_COM-LCH-D2-PDP-ILUMAIONE-T06-S01-G03-G-D-2X_WEB_EN-OC.png'),
            'alt' => 'IQOS ILUMA i ONE albastru deschis',
            'features' => ['Compact', 'Ecran inteligent', 'Mod pauză', 'FlexPuff™', 'Autostart'],
            'usage' => '20 de utilizări',
            'usage_note' => 'per încărcare',
            'price' => '119,00 RON',
            'buy_url' => iqos('/shop-iluma/oferte-iqos/oferte-iluma-i-one'),
        ],
        [
            'id' => 'mid',
            'name' => 'IQOS ILUMA i',
            'tagline' => 'Design iconic, reinventat',
            'image' => cdn('/images/digital/iluma-4-1/PDP/IQOS_ILUMA-I-MID_IILUMAIL24_COM-LCH-D2-PDP-ILUMAIMID-T07-S01-G02-NPNC-D-2X_WEB_EN-OC.png'),
            'alt' => 'Dispozitiv IQOS ILUMA i albastru deschis',
            'features' => ['Ecran inteligent', 'Mod pauză', 'FlexPuff™', 'Autostart'],
            'usage' => 'Până la 3 utilizări',
            'usage_note' => 'consecutive*',
            'price' => '249,00 RON',
            'buy_url' => iqos('/shop-iluma/oferte-iqos/oferte-iluma-i'),
        ],
        [
            'id' => 'prime',
            'name' => 'IQOS ILUMA i PRIME',
            'tagline' => 'Cel mai rafinat și avansat dispozitiv IQOS',
            'image' => cdn('/images/digital/home-rev/Group_427321192.png'),
            'alt' => 'IQOS ILUMA i PRIME Garnet Red',
            'features' => ['Ecran inteligent', 'Mod pauză', 'FlexPuff™', 'Autostart'],
            'usage' => 'Până la 3 utilizări',
            'usage_note' => 'consecutive*',
            'price' => '449,00 RON',
            'buy_url' => iqos('/shop-iluma/oferte-iqos/oferte-iluma-i-prime'),
            'learn_url' => local('iluma-i-prime.php'),
        ],
    ],
    'devices_note' => '*A 3-a experiență nu este garantată și poate varia, în funcție de tiparul de consum al fiecărui utilizator.',

    'refills' => [
        [
            'name' => 'TEREA',
            'count' => '13 variante compatibile',
            'lead' => 'O varietate de gusturi de tutun.',
            'text' => 'Alege varianta potrivită, de la echilibrat la intens.',
            'image' => cdn('/images/digital/home-rev/Component_25.png'),
            'alt' => 'Rezerve IQOS TEREA',
            'learn_url' => iqos('/despre-iqos/descopera-terea'),
            'shop_url' => iqos('/shop-iluma/rezerve-iqos/i-terea'),
        ],
        [
            'name' => 'DELIA',
            'count' => '4 variante compatibile',
            'lead' => 'Gust clasic de tutun.',
            'text' => 'Alege o experiență autentică la un preț avantajos.',
            'image' => cdn('/images/digital/home-rev/Component_26.png'),
            'alt' => 'Rezerve IQOS DELIA',
            'learn_url' => iqos('/descopera-delia'),
            'shop_url' => iqos('/shop-iluma/rezerve-iqos/i-delia'),
        ],
        [
            'name' => 'LEVIA',
            'count' => '5 variante compatibile',
            'lead' => 'Note fresh și fructate, fără tutun.',
            'text' => 'Mixează gustul cu un click.',
            'image' => cdn('/images/digital/home-rev/Component_27.png'),
            'alt' => 'Rezerve IQOS LEVIA',
            'learn_url' => iqos('/descopera-levia'),
            'shop_url' => iqos('/shop-iluma/rezerve-iqos/i-levia'),
        ],
    ],

    // Avantajele cumpărării online (bara „USP” de pe iqos.ro).
    'usps' => [
        ['title' => 'Transport gratuit', 'icon' => cdn('/images/_updates/August2025/USP-homepage/Order-NBI-light.png'), 'text' => 'Comandă online dispozitivul tău IQOS și beneficiezi de livrare gratuită la comenzile de peste 200 RON.'],
        ['title' => 'Retur gratuit', 'icon' => cdn('/images/_updates/August2025/USP-homepage/Approved-NBI-light.png'), 'text' => 'Te-ai răzgândit? Poți returna gratuit dispozitivul în termen de 30 zile.'],
        ['title' => 'Suport pe care te poți baza', 'icon' => cdn('/images/_updates/August2025/USP-homepage/Exceptional-service-NBI-light.png'), 'text' => 'Conectează-te cu noi online, în magazine sau prin telefon.'],
        ['title' => 'Plată prin Klarna', 'icon' => cdn('/images/_updates/August2025/USP-homepage/IQOS_IQOS_PMISFPBHUB_Pay-later-card-64-light_ICONS_EN-OC.png'), 'text' => '3 rate fără dobândă prin Klarna la un coș de minimum 100 RON.'],
        ['title' => 'Livrare rapidă', 'icon' => cdn('/images/_updates/August2025/USP-homepage/24h-delivery-NBI-light.png'), 'text' => 'În București, comenzile plasate luni - vineri până la ora 11 sunt livrate în aceeași zi. Pentru comenzile plasate după această oră sau în alt oraș, livrarea prin curier se face în 1-3 zile lucrătoare.'],
    ],

    'benefits' => [
        ['title' => 'Mai puțin miros persistent', 'text' => 'Mai puțin miros pe mâini, haine și în păr comparativ cu țigările.'],
        ['title' => 'Utilizare intuitivă', 'text' => 'IQOS ILUMA este conceput pentru o utilizare simplă și intuitivă.'],
        ['title' => 'Fără fum', 'text' => 'IQOS nu produce fum și este mai puțin probabil să îi deranjeze pe cei din jur.'],
        ['title' => 'Gust de tutun. Mai multă varietate.', 'text' => 'Descoperă o gamă variată de consumabile, fără scrum.'],
        ['title' => 'IQOS este o alegere mai bună decât continuarea fumatului*', 'text' => 'IQOS încălzește tutunul în loc să îl ardă, oferind o alternativă mai bună pentru fumătorii adulți care altfel ar continua să fumeze.'],
    ],
    'benefits_image' => cdn('/images/digital/home-rev/beneficii_1_1.jpg'),
    'benefits_note' => '*IQOS nu este lipsit de riscuri și conține nicotină, care creează dependență.',

    // Funcțiile ILUMA i, cu ilustrațiile lor.
    'features' => [
        ['title' => 'Ecranul inteligent oferă o experiență intuitivă', 'image' => cdn('/images/digital/home-rev/1.png'), 'alt' => 'Ecranul inteligent al dispozitivului IQOS'],
        ['title' => 'Modul pauză pentru mai multă flexibilitate', 'image' => cdn('/images/_updates/September2026/Group 427321107.png'), 'alt' => 'Modul pauză al dispozitivului IQOS'],
        ['title' => 'Încălzire prin inducție, fără lamelă și fără curățare', 'image' => cdn('/images/_updates/September2026/Group 427321106.png'), 'alt' => 'Tehnologia de încălzire prin inducție IQOS'],
        ['title' => 'Până la 4 pufuri suplimentare cu FlexPuff', 'image' => cdn('/images/_updates/September2026/Group 4273211061.png'), 'alt' => 'Funcția FlexPuff a dispozitivului IQOS'],
    ],
    'how_to_video' => 'https://d3hvodq7wnhe8p.cloudfront.net/images/digital/new-hp-2026/Web_iluma%20i_mid_how_to_use_1920x810px%20(2).mp4',
    'technology_text' => 'IQOS utilizează tehnologia SMARTCORE INDUCTION SYSTEM™, dezvoltată de Philip Morris International. Prin încălzirea tutunului la o temperatură controlată, fără combustie, generează un aerosol care conține nicotină, fără fum, fără scrum și cu mai puțin miros comparativ cu țigările. IQOS nu este lipsit de riscuri și conține nicotină, care creează dependență.',

    'footer' => [
        'columns' => [
            ['title' => 'Linkuri utile', 'links' => [
                ['label' => 'Găsește IQOS', 'url' => iqos('/store/ro/search')],
                ['label' => 'Noutăți', 'url' => iqos('/blog-si-noutati')],
                ['label' => 'Accesează PMI.COM', 'url' => 'https://www.pmi.com/'],
                ['label' => 'Ce este tutunul încălzit', 'url' => iqos('/myiqos/asistenta/ce-este-tutunul-incalzit')],
                ['label' => 'Important', 'url' => iqos('/descopera-iqos/important')],
            ]],
            ['title' => 'Asistență clienți', 'links' => [
                ['label' => 'Contactează-ne', 'url' => iqos('/contacteaza-iqos')],
                ['label' => 'Tutoriale', 'url' => iqos('/iqos-pocket')],
                ['label' => 'Înlocuire IQOS', 'url' => iqos('/myiqos/asistenta/garantie-iqos-si-metode-de-inlocuire')],
                ['label' => 'Garanțiile dispozitivelor IQOS', 'url' => iqos('/garantie')],
                ['label' => 'Întrebări frecvente', 'url' => iqos('/myiqos/asistenta')],
            ]],
            ['title' => 'Legal', 'links' => [
                ['label' => 'ANPC', 'url' => 'https://anpc.ro/'],
                ['label' => 'Termeni și condiții', 'url' => iqos('/conditii-de-utilizare')],
                ['label' => 'Declarație privind accesibilitatea', 'url' => iqos('/declaratie-privind-accesibilitatea')],
                ['label' => 'Notificare privind accesul echitabil la date', 'url' => iqos('/notificare-privind-accesul-echitabil-la-date')],
                ['label' => 'Termeni și condiții - Trade-in', 'url' => iqos('/termeni-si-conditii-trade-in')],
                ['label' => 'Notificare de protecție a datelor', 'url' => 'https://www.pmiprivacy.com/romania/ro/consumer/'],
                ['label' => 'Termeni și condiții - Recomandă un prieten', 'url' => iqos('/regulamentul-oficial-al-campaniei-promotionale-recomanda-iqos-menu')],
                ['label' => 'Termeni și condiții de vânzare', 'url' => iqos('/termeni-si-conditii-de-vanzare')],
                ['label' => 'Termeni și condiții - Auto Delivery', 'url' => iqos('/auto-delivery-termeni-si-conditii')],
                ['label' => 'Termeni și condiții - IQOS Care Plus', 'url' => iqos('/termeni-si-conditii-iqos-care-plus')],
                ['label' => 'Conținut generat de utilizatori', 'url' => iqos('/continut-generat-de-utilizatori')],
                ['label' => 'Notificare de protecție a datelor pentru partenerii de afaceri', 'url' => iqos('/privacy-business-partners')],
            ]],
        ],
        'social' => [
            ['label' => 'Facebook', 'url' => 'https://www.facebook.com/iqos.ro'],
            ['label' => 'Instagram', 'url' => 'https://www.instagram.com/iqos_ro'],
            ['label' => 'YouTube', 'url' => 'https://www.youtube.com/channel/UCO3xTgEvUw7iwwvL-s81s0w'],
        ],
        'company' => [
            'PHILIP MORRIS TRADING S.R.L.',
            'C.U.I. RO4888165, Reg. Com. J2004000511232',
            'Sediu Social: Strada Horia, Cloşca şi Crisan nr. 83 - 105, Otopeni, Clădirea A, Jud. Ilfov, România',
            'Punct de lucru: Str. Nicolae Filipescu, nr. 28, Sector 2, București',
        ],
        'copyright' => '© 2026 Philip Morris Products S.A.',
        'badges' => [
            ['url' => 'https://anpc.ro/ce-este-sal/', 'image' => cdn('/images/digital/pc/SAL-PICTOGRAMA.png'), 'alt' => 'SAL'],
            ['url' => 'https://ec.europa.eu/consumers/odr/main/', 'image' => cdn('/images/digital/pc/pictogramaSOL.png'), 'alt' => 'SOL'],
        ],
    ],
];
