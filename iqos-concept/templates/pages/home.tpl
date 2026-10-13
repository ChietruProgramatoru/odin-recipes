{* Homepage iqos.ro, restilizat. Ordinea secțiunilor urmează site-ul original. *}

{* Hero *}
<section class="hero hero--home section--dark">
    <div class="hero__copy reveal">
        <h1 class="hero__title">{$page.hero.title}<span>{$page.hero.subtitle}</span></h1>
        <div class="hero__actions">
            <a class="button button--large" href="{$page.hero.cta.url}">{$page.hero.cta.label}</a>
            <a class="link-more link-more--large" href="{$page.hero.secondary.url}">{$page.hero.secondary.label}</a>
        </div>
    </div>
    <picture class="hero__media">
        <source media="(max-width: 734px)" srcset="{$page.hero.image_mobile}">
        <img src="{$page.hero.image}" alt="{$page.hero.alt}" fetchpriority="high">
    </picture>
</section>

{* Bannerul DELIA și invitația de login, ca două tile-uri promo *}
<section class="section section--light promos">
    <div class="container promos__grid">
        <article class="promo promo--delia reveal">
            <picture class="promo__media">
                <source media="(max-width: 734px)" srcset="{$page.promo.image_mobile}">
                <img src="{$page.promo.image}" alt="{$page.promo.alt}" loading="lazy">
            </picture>
            <div class="promo__copy">
                <p class="eyebrow">{$page.promo.brand}</p>
                <h2 class="promo__title">{$page.promo.title}</h2>
                <p class="promo__subtitle">{$page.promo.subtitle}</p>
                <a class="button" href="{$page.promo.cta.url}">{$page.promo.cta.label}</a>
            </div>
        </article>

        <article class="promo promo--login reveal">
            <div class="promo__copy">
                <h2 class="promo__title">{$page.login_prompt.title}</h2>
                <p class="promo__subtitle">{$page.login_prompt.text}</p>
                <a class="button" href="{$page.login_prompt.cta.url}">{$page.login_prompt.cta.label}</a>
                <p class="promo__note">{$page.login_prompt.unlock}</p>
            </div>
        </article>
    </div>
    <div class="container">
        <p class="footnote">{$common.sticks_warning}</p>
    </div>
</section>

{* Linkuri rapide, ca rândul de iconițe din „chapter nav” *}
<nav class="chapters section--light" aria-label="Scurtături">
    <ul class="container chapters__list">
        {foreach $page.quick_links as $ql}
            <li><a href="{$ql.url}">{$ql.label}</a></li>
        {/foreach}
    </ul>
</nav>

{include file='sections/usps.tpl'}

{include file='sections/benefits.tpl' title='Beneficiile IQOS'}

{* Ce este IQOS? *}
<section class="section section--light what-is" id="ce-este-iqos">
    <div class="container">
        <header class="section__header reveal">
            <img class="what-is__logo" src="{$page.what_is.logo}" alt="IQOS ILUMA i" loading="lazy">
            <h2 class="headline headline--huge">{$page.what_is.title}</h2>
        </header>
        <p class="statement reveal">
            {$page.what_is.lead}
            <span class="statement__muted">{$page.what_is.lead_rest}</span>
        </p>

        <details class="more reveal">
            <summary><span class="link-more">Vezi mai mult</span></summary>
            <p>
                {$common.technology_text}
                Vezi <a href="{$page.what_is.important_url}">informații importante</a>.
            </p>
        </details>

        {include file='sections/video.tpl'}
        {include file='sections/features.tpl'}
    </div>
</section>

{include file='sections/compare.tpl' theme='gray' highlight=''}

{include file='sections/refills.tpl' theme='dark'}

{* Oferte și programe, cu taburi *}
<section class="section section--light offers" id="oferte">
    <div class="container">
        <header class="section__header section__header--row reveal">
            <div>
                <h2 class="headline">{$page.offers.title}</h2>
                <p class="section__lead">{$page.offers.subtitle}</p>
            </div>
            <div class="segmented" role="tablist" aria-label="Oferte și programe">
                {foreach $page.offers.tabs as $tab}
                    <button class="segmented__button" type="button" role="tab" id="tab-{$tab.id}" aria-controls="panel-{$tab.id}" aria-selected="{if $tab@first}true{else}false{/if}">{$tab.label}</button>
                {/foreach}
            </div>
        </header>
    </div>

    {foreach $page.offers.tabs as $tab}
        <div class="gallery gallery--bleed offers__panel" data-gallery role="tabpanel" id="panel-{$tab.id}" aria-labelledby="tab-{$tab.id}"{if !$tab@first} hidden{/if}>
            <ul class="gallery__track">
                {foreach $tab.items as $item}
                    <li class="offer-card">
                        <div class="offer-card__media">
                            <img src="{$item.image}" alt="" loading="lazy">
                        </div>
                        <div class="offer-card__copy">
                            <p class="eyebrow">{$item.eyebrow}</p>
                            <h3>{$item.title}</h3>
                            <p>{$item.text}</p>
                            <a class="link-more" href="{$item.cta.url}">{$item.cta.label}</a>
                        </div>
                    </li>
                {/foreach}
            </ul>
            <div class="container">
                {include file='sections/gallery-controls.tpl'}
            </div>
        </div>
    {/foreach}
</section>

{* Colaborări *}
<section class="colabs section--dark">
    <img class="colabs__bg" src="{$page.colabs.image}" alt="" loading="lazy">
    <div class="container colabs__inner">
        <div class="colabs__copy reveal">
            <h2 class="headline headline--huge">{$page.colabs.title}</h2>
            <a class="button button--large" href="{$page.colabs.cta.url}">{$page.colabs.cta.label}</a>
        </div>
        <a class="colabs__featured reveal" href="{$page.colabs.cta.url}">
            <img src="{$page.colabs.featured.image}" alt="{$page.colabs.featured.alt}" loading="lazy">
            <span class="eyebrow">{$page.colabs.featured.eyebrow}</span>
            <strong>{$page.colabs.featured.title}</strong>
            <span class="colabs__login">{$page.login_prompt.text}</span>
        </a>
    </div>
</section>

{* Articole utile *}
<section class="section section--gray articles" id="articole">
    <div class="container">
        <header class="section__header section__header--row reveal">
            <h2 class="headline">Articole utile</h2>
        </header>
    </div>
    <div class="gallery gallery--bleed" data-gallery>
        <ul class="gallery__track">
            {foreach $page.articles as $a}
                <li class="article-card">
                    <a href="{$a.url}">
                        <div class="article-card__media"><img src="{$a.image}" alt="" loading="lazy"></div>
                        <div class="article-card__copy">
                            <h3>{$a.title}</h3>
                            <p>{$a.text}</p>
                            <span class="link-more">Citește articolul</span>
                        </div>
                    </a>
                </li>
            {/foreach}
        </ul>
        <div class="container">
            {include file='sections/gallery-controls.tpl'}
        </div>
    </div>
</section>

{* Alte categorii: ZYN și VEEV *}
<section class="section section--light categories">
    <div class="container">
        <header class="section__header reveal">
            <h2 class="headline headline--small">Descoperă alte categorii</h2>
        </header>
        <div class="categories__grid">
            {foreach $page.other_categories as $c}
                <a class="tile categories__tile reveal" href="{$c.url}" rel="noopener">
                    <div>
                        <h3 class="tile__title">{$c.name}</h3>
                        <p class="tile__text">{$c.text}</p>
                        <span class="link-more">Vizitează</span>
                    </div>
                    <img src="{$c.image}" alt="{$c.alt}" loading="lazy">
                </a>
            {/foreach}
        </div>
    </div>
</section>

{* Întrebări frecvente *}
<section class="section section--light faq" id="intrebari">
    <div class="container container--narrow">
        <header class="section__header reveal">
            <h2 class="headline">Întrebări frecvente</h2>
        </header>
        <div class="faq__list">
            {foreach $page.faq as $item}
                <details class="faq__item">
                    <summary>{$item.q}</summary>
                    <p>{$item.a_html nofilter}</p>
                </details>
            {/foreach}
        </div>
    </div>
</section>
