{* Rezervele compatibile: TEREA, DELIA, LEVIA. Parametru: theme. *}
<section class="section section--{$theme|default:'dark'} refills" id="rezerve">
    <div class="container">
        <header class="section__header reveal">
            <h2 class="headline">Rezerve compatibile cu IQOS</h2>
        </header>
        <div class="refills__grid">
            {foreach $common.refills as $r}
                <article class="tile refills__tile reveal">
                    <p class="eyebrow">{$r.count}</p>
                    <h3 class="tile__title">{$r.name}</h3>
                    <p class="tile__text"><strong>{$r.lead}</strong> {$r.text}</p>
                    <div class="tile__actions">
                        <a class="button button--small" href="{$r.shop_url}">Shop</a>
                        <a class="link-more" href="{$r.learn_url}">Află mai multe</a>
                    </div>
                    <img class="refills__image" src="{$r.image}" alt="{$r.alt}" loading="lazy">
                </article>
            {/foreach}
        </div>
        <p class="footnote">{$common.sticks_warning}</p>
    </div>
</section>
