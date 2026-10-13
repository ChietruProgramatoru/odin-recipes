{* Navigația globală, în stilul apple.com: bară subțire, meniuri flyout pe desktop,
   meniu pe tot ecranul pe mobil. *}
<header class="gn" id="gn">
    <nav class="gn__inner" aria-label="Navigație principală">
        <a class="gn__logo" href="{$common.home_url}">
            <img src="{$common.logo}" alt="IQOS" width="52" height="16">
        </a>

        <ul class="gn__list">
            {foreach $common.nav as $item}
                <li class="gn__item{if isset($item.groups)} gn__item--menu{/if}">
                    {if isset($item.groups)}
                        <button class="gn__link" type="button" aria-expanded="false">{$item.label}</button>
                        <div class="gn__flyout">
                            <div class="gn__flyout-inner">
                                {foreach $item.groups as $group}
                                    <div class="gn__group{if !empty($group.big)} gn__group--big{/if}">
                                        <h2 class="gn__group-title">{$group.title}</h2>
                                        <ul>
                                            {foreach $group.links as $link}
                                                <li><a href="{$link.url}">{$link.label}</a></li>
                                            {/foreach}
                                        </ul>
                                    </div>
                                {/foreach}
                            </div>
                        </div>
                    {else}
                        <a class="gn__link" href="{$item.url}">{$item.label}</a>
                    {/if}
                </li>
            {/foreach}
        </ul>

        <div class="gn__actions">
            <a class="gn__icon" href="{$common.account_url}" aria-label="Login / Înregistrează-te">
                <svg viewBox="0 0 24 24" aria-hidden="true"><circle cx="12" cy="8" r="4"/><path d="M4 21c0-4.4 3.6-7 8-7s8 2.6 8 7"/></svg>
            </a>
            <a class="gn__icon" href="{$common.cart_url}" aria-label="Coș de cumpărături">
                <svg viewBox="0 0 24 24" aria-hidden="true"><path d="M5 8h14l-1 13H6L5 8z"/><path d="M9 8V6a3 3 0 0 1 6 0v2"/></svg>
            </a>
            <button class="gn__icon gn__burger" type="button" aria-label="Meniu" aria-expanded="false" aria-controls="gn-mobile">
                <span></span><span></span>
            </button>
        </div>
    </nav>

    <div class="gn__mobile" id="gn-mobile">
        <ul>
            {foreach $common.nav as $item}
                <li>
                    {if isset($item.groups)}
                        <details>
                            <summary>{$item.label}</summary>
                            {foreach $item.groups as $group}
                                <h2 class="gn__group-title">{$group.title}</h2>
                                <ul class="gn__mobile-sub{if !empty($group.big)} gn__mobile-sub--big{/if}">
                                    {foreach $group.links as $link}
                                        <li><a href="{$link.url}">{$link.label}</a></li>
                                    {/foreach}
                                </ul>
                            {/foreach}
                        </details>
                    {else}
                        <a href="{$item.url}">{$item.label}</a>
                    {/if}
                </li>
            {/foreach}
            <li><a href="{$common.account_url}">Login / Înregistrează-te</a></li>
        </ul>
    </div>
</header>
<div class="gn-curtain" aria-hidden="true"></div>
