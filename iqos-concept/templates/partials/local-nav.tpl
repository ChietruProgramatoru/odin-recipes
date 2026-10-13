{* Bara „locală” lipicioasă de sub navigația globală, ca pe paginile de produs Apple. *}
{assign var=ln value=$page.local_nav}
<nav class="ln" id="ln" aria-label="{$ln.title}">
    <div class="ln__inner">
        <a class="ln__title" href="#main">{$ln.title}</a>
        <button class="ln__toggle" type="button" aria-label="Secțiuni" aria-expanded="false" aria-controls="ln-menu">
            <svg viewBox="0 0 12 8" aria-hidden="true"><path d="M1 1.5l5 5 5-5"/></svg>
        </button>
        <div class="ln__menu" id="ln-menu">
            <ul class="ln__links">
                {foreach $ln.links as $link}
                    <li><a href="{$link.url}">{$link.label}</a></li>
                {/foreach}
            </ul>
        </div>
        <a class="button button--small" href="{$ln.cta.url}">{$ln.cta.label}</a>
    </div>
</nav>
