{assign var=gate value=$common.age_gate}
<div class="age-gate" id="age-gate" role="dialog" aria-modal="true" aria-labelledby="age-gate-title">
    <div class="age-gate__panel">
        <img class="age-gate__logo" src="{$common.logo}" alt="IQOS">
        <h1 class="age-gate__title" id="age-gate-title">Ai peste 18 ani?</h1>
        <p class="age-gate__intro">{$gate.intro}</p>

        <form class="age-gate__form" id="age-gate-form" novalidate>
            <div class="age-gate__fields">
                <label class="select">
                    <span class="visually-hidden">Luna nașterii</span>
                    <select name="month" required>
                        <option value="">LUNA</option>
                        {foreach $gate.months as $i => $month}
                            <option value="{$i + 1}">{$month}</option>
                        {/foreach}
                    </select>
                </label>
                <label class="select">
                    <span class="visually-hidden">Anul nașterii</span>
                    <select name="year" required>
                        <option value="">AN</option>
                        {foreach $gate.years as $year}
                            <option value="{$year}">{$year}</option>
                        {/foreach}
                    </select>
                </label>
            </div>
            <p class="age-gate__error" id="age-gate-error" role="alert" hidden>{$gate.error}</p>
            <button class="button button--large" type="submit">Confirmă</button>
        </form>

        <p class="age-gate__disclaimer">
            {$gate.disclaimer}
            Vă rugăm să vizitați pagina <a href="{$gate.important_url}">„Important”</a> pentru informații suplimentare despre riscurile IQOS.
        </p>
    </div>
</div>
