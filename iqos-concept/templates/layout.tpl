<!DOCTYPE html>
<html lang="ro">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>{$page.title}</title>
    <meta name="description" content="{$page.description}">
    <meta name="theme-color" content="#000000">
    <link rel="stylesheet" href="assets/css/style.css">
    {* Ascunde age gate-ul înainte de primul paint dacă vârsta a fost deja confirmată. *}
    <script>{literal}document.documentElement.classList.add('js');try{if(localStorage.getItem('iqos-age-ok')==='1')document.documentElement.classList.add('age-ok')}catch(e){}{/literal}</script>
</head>
<body class="{$page.body_class}">
    <a class="skip-link" href="#main">Accesează conținutul principal</a>

    {include file='partials/age-gate.tpl'}
    {include file='partials/global-nav.tpl'}
    {include file='partials/local-nav.tpl'}

    <main id="main">
        {include file=$page_template}
    </main>

    {include file='partials/footer.tpl'}
    {include file='partials/warning-band.tpl'}

    <script src="assets/js/main.js" defer></script>
</body>
</html>
