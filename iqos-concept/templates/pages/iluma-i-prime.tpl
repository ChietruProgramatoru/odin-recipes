{* Pagina de produs IQOS ILUMA i PRIME, structurată ca pagina iPad Pro. *}

{* Hero *}
<section class="hero hero--product section--dark" id="prezentare">
    <div class="hero__copy reveal">
        <p class="eyebrow eyebrow--accent">{$page.hero.eyebrow}</p>
        <p class="hero__name">{$page.hero.name}</p>
        <h1 class="hero__title hero__title--gradient">{$page.hero.headline}<span>{$page.hero.headline_2}</span></h1>
    </div>
    <div class="hero__product reveal">
        <img src="{$page.hero.image}" alt="{$page.hero.alt}" fetchpriority="high">
    </div>
    <div class="hero__buy reveal">
        <a class="button button--large" href="{$page.hero.cta.url}">{$page.hero.cta.label}</a>
        <p class="hero__price">{$page.hero.price}</p>
    </div>
</section>

{include file='sections/benefits.tpl' title=$page.highlights_title}

{* Design *}
<section class="section section--light design">
    <div class="container">
        <header class="section__header reveal">
            <p class="eyebrow eyebrow--accent">{$page.design.eyebrow}</p>
            <h2 class="headline headline--huge">{$page.design.title}</h2>
        </header>
        <div class="design__media reveal">
            <img src="{$page.design.image}" alt="{$page.design.alt}" loading="lazy">
        </div>
        <p class="statement statement--center reveal">{$page.design.text}</p>
        {include file='sections/features.tpl'}
    </div>
</section>

{* Tehnologie: imagine lipicioasă + pași care trec prin dreptul ei *}
<section class="section section--dark tech" id="tehnologie">
    <div class="container">
        <header class="section__header reveal">
            <p class="eyebrow eyebrow--accent">{$page.technology.eyebrow}</p>
            <h2 class="headline headline--huge">{$page.technology.title}</h2>
        </header>
        <div class="tech__layout">
            <div class="tech__sticky">
                <img src="{$page.technology.image}" alt="Dispozitiv IQOS ILUMA i" loading="lazy">
            </div>
            <ol class="tech__steps">
                {foreach $page.technology.steps as $step}
                    <li class="tech__step reveal">
                        <h3>{$step.title}</h3>
                        <p>{$step.text}</p>
                    </li>
                {/foreach}
            </ol>
        </div>

        <div class="stats">
            {foreach $page.stats as $s}
                <div class="stat reveal">
                    {if $s.prefix}<span class="stat__prefix">{$s.prefix}</span>{/if}
                    <span class="stat__value">{$s.value}</span>
                    <span class="stat__unit">{$s.unit}</span>
                    <span class="stat__label">{$s.label}</span>
                </div>
            {/foreach}
        </div>
        <p class="footnote">{$common.devices_note}</p>
    </div>
</section>

{* Video *}
<section class="section section--light how-to">
    <div class="container">
        <header class="section__header reveal">
            <h2 class="headline">{$page.video.title}</h2>
            <p class="section__lead">{$page.video.text}</p>
        </header>
        {include file='sections/video.tpl'}
    </div>
</section>

{include file='sections/refills.tpl' theme='dark'}

{include file='sections/compare.tpl' theme='gray' highlight='prime'}

{include file='sections/usps.tpl'}

{* Îndemnul final *}
<section class="section section--dark final-cta">
    <div class="container final-cta__inner">
        <div class="final-cta__copy reveal">
            <p class="eyebrow eyebrow--accent">{$page.final_cta.eyebrow}</p>
            <h2 class="headline headline--huge">{$page.final_cta.title}</h2>
            <p class="final-cta__price">{$page.final_cta.price}</p>
            <div class="hero__actions">
                <a class="button button--large" href="{$page.final_cta.cta.url}">{$page.final_cta.cta.label}</a>
                <a class="link-more link-more--large" href="{$page.final_cta.secondary.url}">{$page.final_cta.secondary.label}</a>
            </div>
        </div>
        <img class="final-cta__image reveal" src="{$page.final_cta.image}" alt="" loading="lazy">
    </div>
</section>

<section class="section section--light notes">
    <div class="container">
        <ol class="notes__list">
            <li>{$common.devices_note}</li>
            <li>{$common.benefits_note}</li>
            <li>{$common.sticks_warning}</li>
        </ol>
    </div>
</section>
