<?php

declare(strict_types=1);

// Pagina IQOS ILUMA i PRIME. iqos.ro nu ne-a dat o pagină de produs, așa că
// textele sunt compuse din ce spune homepage-ul despre ILUMA i PRIME.

require_once __DIR__ . '/helpers.php';

$buy = iqos('/shop-iluma/oferte-iqos/oferte-iluma-i-prime');

return [
    'title' => 'IQOS ILUMA i PRIME | IQOS Romania',
    'description' => 'IQOS ILUMA i PRIME – cel mai rafinat și avansat dispozitiv IQOS. Ecran inteligent, Mod pauză, FlexPuff și Autostart.',
    'body_class' => 'page-product',

    'local_nav' => [
        'title' => 'IQOS ILUMA i PRIME',
        'links' => [
            ['label' => 'Prezentare', 'url' => '#prezentare'],
            ['label' => 'Tehnologie', 'url' => '#tehnologie'],
            ['label' => 'Rezerve', 'url' => '#rezerve'],
            ['label' => 'Compară', 'url' => '#compara'],
        ],
        'cta' => ['label' => 'Cumpără', 'url' => $buy],
    ],

    'hero' => [
        'eyebrow' => 'NOU',
        'name' => 'IQOS ILUMA i PRIME',
        'headline' => 'Cel mai rafinat.',
        'headline_2' => 'Cel mai avansat.',
        'image' => cdn('/images/digital/home-rev/Group_427321192.png'),
        'alt' => 'IQOS ILUMA i PRIME Garnet Red',
        'price' => 'De la 449,00 RON',
        'cta' => ['label' => 'Cumpără', 'url' => $buy],
        'secondary' => ['label' => 'Vezi Promo Kit', 'url' => $buy],
    ],

    'highlights_title' => 'Pe scurt.',

    'design' => [
        'eyebrow' => 'Design',
        'title' => 'Proiectat cu eleganță. Se adaptează la ritmul tău.',
        'text' => 'IQOS ILUMA i PRIME este proiectat cu eleganță și dotat cu noi funcții inteligente: Ecran inteligent, Mod pauză, FlexPuff și Autostart.',
        'image' => cdn('/images/_updates/September2026/Promo_Kit_IQOS_ILUMA_I_PRIME_Homepage_1.webp'),
        'alt' => 'IQOS ILUMA i PRIME cu rezerve TEREA',
    ],

    'technology' => [
        'eyebrow' => 'SMARTCORE INDUCTION SYSTEM™',
        'title' => 'Încălzește tutunul. Nu îl arde.',
        'image' => cdn('/images/digital/home-rev/IQOS__IILUMAIL24_COM-TEA-D2-HP--T01-S01-G01-G-D-2X_WEB_EN-OC_1.png'),
        'steps' => [
            ['title' => 'Temperatură controlată.', 'text' => 'Prin încălzirea tutunului la o temperatură controlată, fără combustie, IQOS generează un aerosol care conține nicotină.'],
            ['title' => 'Fără fum. Fără scrum.', 'text' => 'Fără fum, fără scrum și cu mai puțin miros comparativ cu țigările.'],
            ['title' => 'Fără lamelă. Fără curățare.', 'text' => 'Încălzirea prin inducție înseamnă că nu există lamelă care să trebuiască curățată.'],
            ['title' => 'Dezvoltat de Philip Morris International.', 'text' => 'IQOS nu este lipsit de riscuri și conține nicotină, care creează dependență.'],
        ],
    ],

    'stats' => [
        ['prefix' => 'Până la', 'value' => '4', 'unit' => 'pufuri', 'label' => 'suplimentare cu FlexPuff™'],
        ['prefix' => 'Până la', 'value' => '3', 'unit' => 'utilizări', 'label' => 'consecutive*'],
        ['prefix' => '', 'value' => '13', 'unit' => 'variante', 'label' => 'de rezerve TEREA compatibile'],
        ['prefix' => '', 'value' => '30', 'unit' => 'de zile', 'label' => 'pentru retur gratuit'],
    ],

    'video' => [
        'title' => 'Simplu de folosit. Din prima.',
        'text' => 'IQOS ILUMA este conceput pentru o utilizare simplă și intuitivă.',
    ],

    'final_cta' => [
        'eyebrow' => 'Promo Kit IQOS ILUMA i PRIME',
        'title' => 'Dispozitivul, plus 2 pachete de rezerve la alegere.',
        'image' => cdn('/images/_updates/September2026/Promo_Kit_IQOS_ILUMA_I_PRIME_Homepage_1.webp'),
        'price' => '449,00 RON',
        'cta' => ['label' => 'Cumpără', 'url' => $buy],
        'secondary' => ['label' => 'Testează gratuit în magazine', 'url' => iqos('/incearca-iqos')],
    ],
];
