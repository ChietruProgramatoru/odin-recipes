<?php

declare(strict_types=1);

require __DIR__ . '/vendor/autoload.php';

use Smarty\Smarty;

/**
 * Randează o pagină în layout-ul comun.
 *
 * $page este numele fișierului din data/ și templates/pages/ (fără extensie).
 */
function render(string $page): void
{
    $smarty = new Smarty();
    $smarty->setTemplateDir(__DIR__ . '/templates')
        ->setCompileDir(__DIR__ . '/templates_c')
        ->setEscapeHtml(true);

    $smarty->assign('common', require __DIR__ . '/data/common.php');
    $smarty->assign('page', require __DIR__ . "/data/{$page}.php");
    $smarty->assign('page_template', "pages/{$page}.tpl");

    $smarty->display('layout.tpl');
}
