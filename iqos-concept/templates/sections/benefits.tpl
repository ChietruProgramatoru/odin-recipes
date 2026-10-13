{* Beneficiile IQOS ca galerie „Pe scurt” (Get the highlights). Parametru: title. *}
<section class="section section--dark highlights" id="beneficii">
    <div class="container">
        <header class="section__header section__header--row reveal">
            <h2 class="headline">{$title|default:'Beneficiile IQOS'}</h2>
        </header>
    </div>
    <div class="gallery gallery--bleed" data-gallery>
        <ul class="gallery__track">
            {foreach $common.benefits as $b}
                <li class="highlight-card">
                    <img class="highlight-card__image" src="{$common.benefits_image}" alt="" loading="lazy">
                    <div class="highlight-card__copy">
                        <p class="eyebrow">Beneficiile IQOS</p>
                        <h3>{$b.title}</h3>
                        <p>{$b.text}</p>
                    </div>
                </li>
            {/foreach}
        </ul>
        <div class="container">
            {include file='sections/gallery-controls.tpl'}
        </div>
    </div>
    <div class="container">
        <p class="footnote">{$common.benefits_note}</p>
    </div>
</section>
