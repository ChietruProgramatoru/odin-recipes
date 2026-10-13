{* De ce să cumperi de pe iqos.ro – rând de avantaje, ca „De ce să cumperi de la Apple”. *}
<section class="section section--light usps">
    <div class="container">
        <header class="section__header section__header--row reveal">
            <h2 class="headline headline--small">De ce să cumperi IQOS online.</h2>
        </header>
        <div class="gallery" data-gallery>
            <ul class="gallery__track usps__track">
                {foreach $common.usps as $u}
                    <li class="usps__card">
                        <img class="usps__icon" src="{$u.icon}" alt="" loading="lazy" width="48" height="48">
                        <h3>{$u.title}</h3>
                        <p>{$u.text}</p>
                    </li>
                {/foreach}
            </ul>
            {include file='sections/gallery-controls.tpl'}
        </div>
    </div>
</section>
