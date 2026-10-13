{assign var=f value=$common.footer}
<section class="health" aria-label="Avertisment">
    <div class="container">
        <p class="health__text">{$common.health_warning}</p>
        <p class="health__eu">
            <span>{$common.eu_warning.source}</span>
            <strong>{$common.eu_warning.text}</strong>
        </p>
    </div>
</section>

<footer class="footer">
    <div class="container">
        <div class="footer__columns">
            {foreach $f.columns as $col}
                <div class="footer__col">
                    <details class="footer__details" open>
                        <summary><h2>{$col.title}</h2></summary>
                        <ul>
                            {foreach $col.links as $link}
                                <li><a href="{$link.url}">{$link.label}</a></li>
                            {/foreach}
                        </ul>
                    </details>
                </div>
            {/foreach}
            <div class="footer__col">
                <h2>Urmărește-ne</h2>
                <ul>
                    {foreach $f.social as $s}
                        <li><a href="{$s.url}" rel="noopener">{$s.label}</a></li>
                    {/foreach}
                </ul>
            </div>
        </div>

        <div class="footer__bottom">
            <div class="footer__company">
                {foreach $f.company as $line}
                    <p>{$line}</p>
                {/foreach}
                <p>{$f.copyright}</p>
            </div>
            <div class="footer__badges">
                {foreach $f.badges as $b}
                    <a href="{$b.url}" rel="noopener"><img src="{$b.image}" alt="{$b.alt}" loading="lazy" width="150" height="38"></a>
                {/foreach}
            </div>
        </div>
    </div>
</footer>
