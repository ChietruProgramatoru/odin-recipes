{* „Compară dispozitivele” – coloanele de comparație în stilul „Care iPad e pentru tine?”.
   Parametri: theme (light/dark), highlight (id-ul dispozitivului evidențiat, opțional). *}
<section class="section section--{$theme|default:'light'} compare" id="compara">
    <div class="container">
        <header class="section__header reveal">
            <h2 class="headline">Compară dispozitivele</h2>
            <p class="section__lead">Ce dispozitiv de tutun încălzit ți se potrivește?</p>
        </header>

        <div class="compare__grid">
            {foreach $common.devices as $d}
                <article class="compare__col reveal{if isset($highlight) && $highlight == $d.id} compare__col--active{/if}">
                    <div class="compare__image">
                        <img src="{$d.image}" alt="{$d.alt}" loading="lazy">
                    </div>
                    <p class="eyebrow eyebrow--accent">NOU</p>
                    <h3 class="compare__name">{$d.name}</h3>
                    <p class="compare__tagline">{$d.tagline}</p>
                    <p class="compare__price">{$d.price}</p>
                    <div class="compare__actions">
                        <a class="button" href="{$d.buy_url}">Cumpără</a>
                        {if isset($d.learn_url)}
                            <a class="link-more" href="{$d.learn_url}">Află mai multe</a>
                        {/if}
                    </div>
                    <hr>
                    <ul class="compare__features">
                        {foreach $d.features as $feature}
                            <li>{$feature}</li>
                        {/foreach}
                    </ul>
                    <p class="compare__usage"><strong>{$d.usage}</strong> <span>{$d.usage_note}</span></p>
                </article>
            {/foreach}
        </div>
        <p class="footnote">{$common.devices_note}</p>
    </div>
</section>
