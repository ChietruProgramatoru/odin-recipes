<?php

declare(strict_types=1);

// Conținutul homepage-ului, preluat de pe iqos.ro.

require_once __DIR__ . '/helpers.php';

return [
    'title' => 'Dispozitive IQOS ILUMA si IQOS ILUMA i | IQOS Romania',
    'description' => 'Descoperă IQOS, alternativa fără fum la țigări, care încălzește tutunul în loc să îl ardă.',
    'body_class' => 'page-home',

    'local_nav' => [
        'title' => 'IQOS',
        'links' => [
            ['label' => 'Ce este IQOS?', 'url' => '#ce-este-iqos'],
            ['label' => 'Dispozitive', 'url' => '#compara'],
            ['label' => 'Rezerve', 'url' => '#rezerve'],
            ['label' => 'Oferte', 'url' => '#oferte'],
            ['label' => 'Întrebări', 'url' => '#intrebari'],
        ],
        'cta' => ['label' => 'Cumpără', 'url' => iqos('/shop-iluma/disp-iluma')],
    ],

    'hero' => [
        'title' => 'Curios',
        'subtitle' => 'să afli mai mult?',
        'image' => cdn('/images/_updates/September2026/Banner_homepage_LAS_3840x1620.png'),
        'image_mobile' => cdn('/images/digital/home-rev/Landing_page_ILUMA_i_MID_homepage_mobile_1125x4151_29px_1.png'),
        'alt' => 'O imagine cu două pachete de rezerve LEVIA și un dispozitiv din generația IQOS ILUMA',
        'cta' => ['label' => 'Shop', 'url' => iqos('/shop-iluma/disp-iluma')],
        'secondary' => ['label' => 'Descoperă IQOS ILUMA i PRIME', 'url' => local('iluma-i-prime.php')],
    ],

    'promo' => [
        'title' => 'Gust clasic de tutun',
        'subtitle' => 'Preț avantajos',
        'brand' => 'DELIA',
        'image' => cdn('/images/digital/DELIA/DELIA_Banner_29092026/Desktop_banner.png'),
        'image_mobile' => cdn('/images/digital/DELIA/DELIA_Banner_29092026/Banner_Delia_mobile.png'),
        'alt' => 'Imagine de fundal cu un bărbat care privește un oraș, totul în nuanțe de roșu Delia Red',
        'cta' => ['label' => 'Cumpără', 'url' => iqos('/shop-iluma/rezerve-iqos/i-delia')],
    ],

    'login_prompt' => [
        'title' => 'Curios?',
        'text' => 'Intră în cont pentru a vedea tot conținutul.',
        'cta' => ['label' => 'Log in', 'url' => iqos('/auth-loginform')],
        'unlock' => 'Intră în cont sau înregistrează-te pentru a debloca experiența completă',
    ],

    'quick_links' => [
        ['label' => 'Ce este IQOS?', 'url' => '#ce-este-iqos'],
        ['label' => 'Rezerve compatibile', 'url' => '#rezerve'],
        ['label' => 'Oferte & Programe', 'url' => '#oferte'],
    ],

    'what_is' => [
        'title' => 'Ce este IQOS?',
        'lead' => 'Există și alte moduri de a utiliza tutunul. Descoperă IQOS, alternativa fără fum la țigări, care încălzește tutunul în loc să îl ardă.',
        'lead_rest' => 'O experiență diferită, creată pentru adulții care altfel ar continua să fumeze sau să utilizeze produse cu nicotină.',
        'important_url' => iqos('/descopera-iqos/important'),
        'logo' => cdn('/images/_updates/September2026/IQOS_IQOS_PMISFPBHUB_IQOS-LOGO-VERTICAL-KEY-TUR-POS-RGB_LOGO_EN-OC.png'),
        'device_image' => cdn('/images/digital/home-rev/IQOS__IILUMAIL24_COM-TEA-D2-HP--T01-S01-G01-G-D-2X_WEB_EN-OC_1.png'),
    ],

    'offers' => [
        'title' => 'Oferte și programe IQOS',
        'subtitle' => 'Descoperă ofertele și programele pentru tine',
        'image' => cdn('/images/digital/home-rev/16548_607_BH_NEVER-STILL_ONE_PAPNC_1.png'),
        'tabs' => [
            [
                'id' => 'oferte',
                'label' => 'Oferte',
                'items' => [
                    [
                        'eyebrow' => 'Promo Kit',
                        'title' => 'IQOS ILUMA i',
                        'text' => 'Dispozitiv IQOS ILUMA i cu 2 pachete de rezerve la alegere. Design iconic, baterie de lungă durată și experiență premium. Livrare gratuită.',
                        'image' => cdn('/images/_updates/September2026/Promo_Kit_IQOS_ILUMA_I_Homepage_1.webp'),
                        'cta' => ['label' => 'Cumpără', 'url' => iqos('/shop-iluma/oferte-iqos/oferte-iluma-i')],
                    ],
                    [
                        'eyebrow' => 'Promo Kit',
                        'title' => 'IQOS ILUMA i ONE',
                        'text' => 'Dispozitiv IQOS ILUMA i ONE cu 2 pachete de rezerve la alegere. Design elegant și compact, all-in-one, potrivit pentru un stil de viață activ.',
                        'image' => cdn('/images/_updates/September2026/Promo_Kit_IQOS_ILUMA_I_ONE_Homepage_1.webp'),
                        'cta' => ['label' => 'Cumpără', 'url' => iqos('/shop-iluma/oferte-iqos/oferte-iluma-i-one')],
                    ],
                    [
                        'eyebrow' => 'Promo Kit',
                        'title' => 'IQOS ILUMA i PRIME',
                        'text' => 'Dispozitiv IQOS ILUMA i PRIME cu 2 pachete de rezerve la alegere. Proiectat cu eleganță și dotat cu noi funcții inteligente, se adaptează la ritmul tău.',
                        'image' => cdn('/images/_updates/September2026/Promo_Kit_IQOS_ILUMA_I_PRIME_Homepage_1.webp'),
                        'cta' => ['label' => 'Cumpără', 'url' => iqos('/shop-iluma/oferte-iqos/oferte-iluma-i-prime')],
                    ],
                    [
                        'eyebrow' => 'Alege pachetul tău',
                        'title' => 'Pachet TEREA',
                        'text' => 'Alege-ți varianta dorită de pachet TEREA, în aromele tale preferate. Compatibile exclusiv cu dispozitivele din gama IQOS ILUMA.',
                        'image' => cdn('/images/digital/home-rev/terea.png'),
                        'cta' => ['label' => 'Vezi pachetele', 'url' => iqos('/shop-iluma/rezerve-iqos/i-terea/pachet-terea')],
                    ],
                ],
            ],
            [
                'id' => 'programe',
                'label' => 'Programe',
                'items' => [
                    [
                        'eyebrow' => 'În magazine',
                        'title' => 'Testează IQOS gratuit în magazine',
                        'text' => 'O experiență ghidată, cu recomandări personalizate.',
                        'image' => cdn('/images/_updates/September2026/iqos_56_2_1.webp'),
                        'cta' => ['label' => 'Încearcă', 'url' => iqos('/incearca-iqos')],
                    ],
                    [
                        'eyebrow' => 'Program',
                        'title' => 'Primii paşi cu IQOS',
                        'text' => 'Programul prin care un consultant îți este alături în primele zile, ca să te bucuri de fiecare moment cu IQOS și să descoperi surprizele pregătite pentru tine.',
                        'image' => cdn('/images/digital/home-rev/BOY_mai_mare_1_2.png'),
                        'cta' => ['label' => 'Află mai multe', 'url' => iqos('/incearca-iqos')],
                    ],
                    [
                        'eyebrow' => 'Loialitate',
                        'title' => 'IQOS CLUB',
                        'text' => 'Un univers plin de recompense și beneficii exclusive. Descoperă o experiență creată special pentru tine.',
                        'image' => cdn('/images/_updates/September2026/Group 427321133.png'),
                        'cta' => ['label' => 'Intră în club', 'url' => iqos('/auth-loginform?loyalty=1')],
                    ],
                ],
            ],
        ],
    ],

    'colabs' => [
        'title' => 'Experiențe. Comunitate. Inspirație.',
        'image' => cdn('/images/digital/home-rev/Component_63_1.png'),
        'cta' => ['label' => 'Descoperă colaborările', 'url' => iqos('/colaborari-iqos')],
        'featured' => [
            'eyebrow' => 'Colaborare',
            'title' => 'IQOS x fratelli',
            'image' => cdn('/images/digital/campaign/2026/terea-x-fratelli/Group_427321112.png'),
            'logo' => cdn('/images/digital/campaign/2026/terea-x-fratelli/Logo_IQOS_x_fratelli.png'),
            'alt' => 'Vizual campanie TEREA x Fratelli',
        ],
    ],

    'articles' => [
        ['title' => 'IQOS ILUMA i REMIX și TEREA REMASTER Ediție Limitată', 'text' => 'Vara nu este niciodată aceeași – e în continuă mișcare, se transformă, evoluează...', 'image' => cdn('/images/detailed/21/Header_Article.png'), 'url' => iqos('/noutatile-iqos/iqos-iluma-i-remix-terea-remaster-editie-limitata')],
        ['title' => 'Electric Purple: noua culoare a gamei IQOS ILUMA i este aici', 'text' => 'IQOS ILUMA i Electric Purple marchează lansarea unei noi opțiuni de culoare permanente...', 'image' => cdn('/images/detailed/21/IQOS__IIIEP25_WEB-DX-HP-T01-S01-G01-NPNC-D-2X_WEB_EN-OC_1.png'), 'url' => iqos('/noutatile-iqos/electric-purple-noua-culoare-gama-iqos-iluma')],
        ['title' => 'Heat-not-Burn: Care sunt asemănările și deosebirile între dispozitivele IQOS ILUMA și IQOS ILUMA i?', 'text' => 'Atât IQOS ILUMA, cât și IQOS ILUMA i funcționează cu ajutorul sistemului inovator...', 'image' => cdn('/images/detailed/21/IQOS_ILUMA-I-PRIME_IILUMAIL24_GSU-LCH-D2-TROUBLE-ILUMAIPRIME-T01-S01-G01-PNC-D-1X_WEB_EN-OC_1_1.png'), 'url' => iqos('/noutatile-iqos/heat-not-burn-deosebirile-intre-dispozitivele-iqos-iluma-iqos-iluma-i')],
        ['title' => 'Reguli de bune maniere cu dispozitivul IQOS în spațiile publice închise și deschise', 'text' => 'În România, țigările convenționale, țigările electronice și produsele asociate lor...', 'image' => cdn('/images/detailed/21/IQOS_ILUMA-I-MID_IILUMAIL24_GSU-LCH-D2-TROUBLE-ILUMAIMID-T01-S01-G01-PNC-D-2X_WEB_EN-OC_1.jpg'), 'url' => iqos('/noutatile-iqos/reguli-de-bune-maniere-cu-iqos')],
        ['title' => 'Descoperă noua colecție IQOS ILUMA i x Seletti în ediție limitată', 'text' => 'Ce se întâmplă atunci când un provocator al designului întâlnește un dispozitiv...', 'image' => cdn('/images/detailed/21/selettismallerpic2.jpg'), 'url' => iqos('/noutatile-iqos/descopera-noua-colectie-iqos-iluma-i-x-seletti-in-editie-limitata')],
        ['title' => 'IQOS ILUMA x Seletti', 'text' => 'Brandul italian care a redefinit obiectele cotidiene, de la lămpi și oglinzi până la...', 'image' => cdn('/images/detailed/21/Selettifinalversionpre.jpg'), 'url' => iqos('/noutatile-iqos/iqos-iluma-x-seletti')],
    ],

    'other_categories' => [
        ['name' => 'Descoperă ZYN', 'text' => 'Pliculețe cu nicotină', 'image' => cdn('/images/digital/zyn/generic_799x800.png'), 'alt' => 'Pachet de ZYN', 'url' => 'https://www.zyn.com/ro/'],
        ['name' => 'Descoperă VEEV', 'text' => 'Țigaretă electronică', 'image' => cdn('/images/digital/VEEV/downtime/veev_device3.png'), 'alt' => 'Dispozitivul VEEV ONE', 'url' => 'http://veev-vape.com/ro/'],
    ],

    // Răspunsurile pot conține linkuri, de aceea sunt HTML (scris de noi, nu de utilizatori).
    'faq' => [
        ['q' => 'De unde pot să cumpăr IQOS?', 'a_html' => 'Poți cumpăra IQOS online, direct de pe iqos.ro, din magazinele IQOS sau din magazinele partenere. Utilizează <a href="' . iqos('/store/store-locator') . '">localizatorul nostru de magazine</a> și descoperă cel mai apropiat magazin.'],
        ['q' => 'Cât costă un dispozitiv IQOS?', 'a_html' => 'Prețul dispozitivelor variază în funcție de opțiunea aleasă. Aceste prețuri sunt valabile pe iqos.ro și în insulele și magazinele IQOS. Prețul poate varia în alte canale de revânzare în funcție de politicile comerciale ale retailerilor.'],
        ['q' => 'Pot testa dispozitivul IQOS înainte de a face achiziția?', 'a_html' => 'Da. Poți testa dispozitivul IQOS înainte de a-l cumpăra în locațiile IQOS. Folosește harta locațiilor și găsește <a href="' . iqos('/store/store-locator') . '">cel mai apropiat magazin IQOS</a>.'],
        ['q' => 'Unde pot utiliza dispozitivele IQOS?', 'a_html' => 'Poți utiliza IQOS în locurile în care legislația și regulile locației permit acest lucru. În spațiile private decizia aparține proprietarului, iar în spațiile publice este recomandat să verifici regulile aplicabile înainte de utilizare. Citește <a href="' . iqos('/noutatile-iqos/reguli-de-bune-maniere-cu-iqos') . '">regulile de bune maniere cu IQOS</a>.'],
    ],
];
