# iqos.ro în stilul Apple iPad Pro

Homepage-ul iqos.ro și o pagină de produs IQOS ILUMA i PRIME, restilizate după
[apple.com/ro/ipad-pro](https://www.apple.com/ro/ipad-pro/): tipografie mare,
secțiuni negre și albe alternate, navigație flyout, bară locală lipicioasă,
galerii cu scroll-snap și animații la scroll. Accentul e turcoazul IQOS.

## Pornire

Ai nevoie de PHP 8.1+ și Composer.

```sh
cd iqos-concept
composer install
php -S localhost:8000 -t public
```

Deschide <http://localhost:8000> (homepage) și
<http://localhost:8000/iluma-i-prime.php> (pagina de produs).

## Structură

```
bootstrap.php          render(): creează Smarty și randează o pagină în layout
public/                rădăcina serverului
  index.php            homepage
  iluma-i-prime.php    pagina de produs
  assets/css/style.css toate stilurile
  assets/js/main.js    age gate, meniuri, galerii, taburi, video, animații
data/                  conținutul, ca array-uri PHP
  common.php           navigație, footer, avertismente, dispozitive, rezerve
  home.php             conținutul homepage-ului
  iluma-i-prime.php    conținutul paginii de produs
  helpers.php          iqos() / cdn() / local() pentru linkuri și imagini
templates/
  layout.tpl           scheletul paginii
  partials/            age gate, navigații, footer, banda de avertisment
  sections/            secțiuni folosite pe ambele pagini (compară, rezerve...)
  pages/               câte un template pe pagină
```

O pagină nouă înseamnă trei fișiere: `data/<pagina>.php`, `templates/pages/<pagina>.tpl`
și `public/<pagina>.php` care apelează `render('<pagina>')`.

## Note

- Textele homepage-ului sunt preluate din sursa iqos.ro. Pagina ILUMA i PRIME e
  compusă din ce spune homepage-ul despre acest dispozitiv.
- Imaginile și clipul se încarcă direct de pe CDN-ul IQOS (cloudfront). Dacă un
  fișier dispare de acolo, înlocuiește URL-ul în `data/`.
- Magazinul, contul, coșul și restul linkurilor duc pe iqos.ro real. Proiectul
  nu are coș, login sau checkout.
- Scripturile de tracking ale site-ului original (OneTrust, Optimizely, GTM) au
  fost scoase.
- Confirmarea vârstei se salvează în `localStorage` (`iqos-age-ok`). Șterge
  cheia ca să vezi din nou age gate-ul.
