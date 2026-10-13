<?php

declare(strict_types=1);

/** Link către o pagină de pe iqos.ro real (magazin, cont, suport...). */
function iqos(string $path): string
{
    return 'https://www.iqos.ro' . $path;
}

/** Imagine de pe CDN-ul IQOS. Spațiile din nume sunt codate. */
function cdn(string $path): string
{
    return 'https://d1dpf5qi8okjv1.cloudfront.net' . str_replace(' ', '%20', $path);
}

/** Link către una dintre paginile acestui proiect. */
function local(string $file): string
{
    return $file;
}
