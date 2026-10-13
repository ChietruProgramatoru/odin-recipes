{* Cele patru funcții ILUMA i, ca grilă de tile-uri. *}
<div class="features">
    {foreach $common.features as $feat}
        <figure class="tile features__tile reveal">
            <img src="{$feat.image}" alt="{$feat.alt}" loading="lazy">
            <figcaption>{$feat.title}</figcaption>
        </figure>
    {/foreach}
</div>
