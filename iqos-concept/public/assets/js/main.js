// Interacțiunile paginii: age gate, navigație, galerii, taburi, video, animații la scroll.
(function () {
    'use strict';

    document.documentElement.classList.add('js');

    var mobileQuery = window.matchMedia('(max-width: 734px)');
    var reducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)');

    function store(key, value) {
        try {
            if (value === undefined) return localStorage.getItem(key);
            localStorage.setItem(key, value);
        } catch (e) {
            return null;
        }
    }

    /* --- Age gate -------------------------------------------------------- */

    function initAgeGate() {
        var gate = document.getElementById('age-gate');
        var form = document.getElementById('age-gate-form');
        if (!gate || !form) return;

        if (document.documentElement.classList.contains('age-ok')) return;
        form.elements.month.focus();

        form.addEventListener('submit', function (event) {
            event.preventDefault();
            var month = parseInt(form.elements.month.value, 10);
            var year = parseInt(form.elements.year.value, 10);
            var error = document.getElementById('age-gate-error');

            if (!month || !year) {
                error.textContent = 'Selectează luna și anul nașterii.';
                error.hidden = false;
                return;
            }

            // Fără zi, considerăm ziua de 1 a lunii: cine împlinește 18 ani luna aceasta intră.
            var now = new Date();
            var age = now.getFullYear() - year - (now.getMonth() + 1 < month ? 1 : 0);
            if (age < 18) {
                error.textContent = error.dataset.message || error.textContent;
                error.hidden = false;
                return;
            }

            store('iqos-age-ok', '1');
            gate.classList.add('is-leaving');
            setTimeout(function () {
                document.documentElement.classList.add('age-ok');
            }, 500);
        });

        var error = document.getElementById('age-gate-error');
        error.dataset.message = error.textContent;
    }

    /* --- Navigația globală -------------------------------------------- */

    function initGlobalNav() {
        var gn = document.getElementById('gn');
        if (!gn) return;

        var menus = gn.querySelectorAll('.gn__item--menu');
        var closeTimer;

        function setOpen(item, open) {
            item.classList.toggle('is-open', open);
            item.querySelector('.gn__link').setAttribute('aria-expanded', String(open));
        }

        function closeAll() {
            menus.forEach(function (item) { setOpen(item, false); });
            document.body.classList.remove('gn-flyout-open');
        }

        function open(item) {
            clearTimeout(closeTimer);
            menus.forEach(function (other) { if (other !== item) setOpen(other, false); });
            setOpen(item, true);
            document.body.classList.add('gn-flyout-open');
        }

        menus.forEach(function (item) {
            item.addEventListener('mouseenter', function () { open(item); });
            item.addEventListener('mouseleave', function () {
                closeTimer = setTimeout(closeAll, 120);
            });
            item.querySelector('.gn__link').addEventListener('click', function () {
                if (item.classList.contains('is-open')) closeAll();
                else open(item);
            });
            item.addEventListener('focusout', function (event) {
                if (!item.contains(event.relatedTarget)) {
                    setOpen(item, false);
                    if (!gn.querySelector('.is-open')) document.body.classList.remove('gn-flyout-open');
                }
            });
        });

        document.addEventListener('keydown', function (event) {
            if (event.key === 'Escape') closeAll();
        });
        document.querySelector('.gn-curtain').addEventListener('click', closeAll);

        // Meniul mobil
        var burger = gn.querySelector('.gn__burger');
        burger.addEventListener('click', function () {
            var isOpen = gn.classList.toggle('gn--open');
            burger.setAttribute('aria-expanded', String(isOpen));
            burger.setAttribute('aria-label', isOpen ? 'Închide meniul' : 'Meniu');
            document.body.classList.toggle('gn-mobile-open', isOpen);
        });
    }

    /* --- Navigația locală ----------------------------------------------- */

    function initLocalNav() {
        var ln = document.getElementById('ln');
        if (!ln) return;

        var toggle = ln.querySelector('.ln__toggle');
        var links = Array.prototype.slice.call(ln.querySelectorAll('.ln__links a'));

        function close() {
            ln.classList.remove('ln--open');
            toggle.setAttribute('aria-expanded', 'false');
        }

        toggle.addEventListener('click', function () {
            var isOpen = ln.classList.toggle('ln--open');
            toggle.setAttribute('aria-expanded', String(isOpen));
        });
        links.forEach(function (link) { link.addEventListener('click', close); });

        // Evidențiază secțiunea curentă.
        if (!('IntersectionObserver' in window)) return;
        var byId = {};
        links.forEach(function (link) {
            var target = document.querySelector(link.getAttribute('href'));
            if (target) byId[target.id] = link;
        });
        var observer = new IntersectionObserver(function (entries) {
            entries.forEach(function (entry) {
                if (!entry.isIntersecting) return;
                links.forEach(function (link) { link.classList.remove('is-current'); });
                byId[entry.target.id].classList.add('is-current');
            });
        }, { rootMargin: '-45% 0px -50% 0px' });
        Object.keys(byId).forEach(function (id) { observer.observe(document.getElementById(id)); });
    }

    /* --- Galerii orizontale ------------------------------------------- */

    function initGallery(gallery) {
        var track = gallery.querySelector('.gallery__track');
        var items = Array.prototype.slice.call(track.children);
        var dotsBox = gallery.querySelector('.gallery__dots');
        var prev = gallery.querySelector('[data-dir="-1"]');
        var next = gallery.querySelector('[data-dir="1"]');
        if (!items.length) return;

        var dots = items.map(function (item, i) {
            var dot = document.createElement('button');
            dot.type = 'button';
            dot.className = 'gallery__dot';
            dot.setAttribute('role', 'tab');
            dot.setAttribute('aria-label', 'Slide ' + (i + 1));
            dot.addEventListener('click', function () { goTo(i); });
            dotsBox.appendChild(dot);
            return dot;
        });

        function padding() {
            return parseFloat(getComputedStyle(track).paddingLeft) || 0;
        }

        function goTo(i) {
            i = Math.max(0, Math.min(items.length - 1, i));
            track.scrollTo({
                left: items[i].offsetLeft - track.offsetLeft - padding(),
                behavior: reducedMotion.matches ? 'auto' : 'smooth'
            });
        }

        function current() {
            var x = track.scrollLeft + padding();
            var best = 0;
            items.forEach(function (item, i) {
                if (Math.abs(item.offsetLeft - track.offsetLeft - x) < Math.abs(items[best].offsetLeft - track.offsetLeft - x)) best = i;
            });
            return best;
        }

        function update() {
            var index = current();
            var atEnd = track.scrollLeft + track.clientWidth >= track.scrollWidth - 4;
            dots.forEach(function (dot, i) {
                dot.setAttribute('aria-selected', String(atEnd ? i === items.length - 1 : i === index));
            });
            prev.disabled = track.scrollLeft <= 4;
            next.disabled = atEnd;
        }

        prev.addEventListener('click', function () { goTo(current() - 1); });
        next.addEventListener('click', function () { goTo(current() + 1); });

        var frame;
        track.addEventListener('scroll', function () {
            cancelAnimationFrame(frame);
            frame = requestAnimationFrame(update);
        }, { passive: true });
        window.addEventListener('resize', update);
        gallery.galleryUpdate = update;
        update();
    }

    /* --- Taburi (Oferte / Programe) ----------------------------------- */

    function initTabs() {
        document.querySelectorAll('[role="tablist"].segmented').forEach(function (list) {
            var tabs = Array.prototype.slice.call(list.querySelectorAll('[role="tab"]'));
            function select(tab) {
                tabs.forEach(function (t) {
                    var selected = t === tab;
                    t.setAttribute('aria-selected', String(selected));
                    t.tabIndex = selected ? 0 : -1;
                    var panel = document.getElementById(t.getAttribute('aria-controls'));
                    panel.hidden = !selected;
                    if (selected && panel.galleryUpdate) panel.galleryUpdate();
                });
            }
            tabs.forEach(function (tab, i) {
                tab.tabIndex = i === 0 ? 0 : -1;
                tab.addEventListener('click', function () { select(tab); });
                tab.addEventListener('keydown', function (event) {
                    var step = event.key === 'ArrowRight' ? 1 : event.key === 'ArrowLeft' ? -1 : 0;
                    if (!step) return;
                    var nextTab = tabs[(i + step + tabs.length) % tabs.length];
                    select(nextTab);
                    nextTab.focus();
                });
            });
        });
    }

    /* --- Video ----------------------------------------------------------- */

    function initVideos() {
        document.querySelectorAll('.video').forEach(function (box) {
            var video = box.querySelector('video');
            var button = box.querySelector('.video__toggle');
            var userPaused = reducedMotion.matches;

            function sync() {
                box.classList.toggle('is-paused', video.paused);
                button.setAttribute('aria-label', video.paused ? 'Redă' : 'Pauză');
            }

            button.addEventListener('click', function () {
                if (video.paused) { userPaused = false; video.play(); }
                else { userPaused = true; video.pause(); }
            });
            video.addEventListener('play', sync);
            video.addEventListener('pause', sync);
            sync();

            if (!('IntersectionObserver' in window)) return;
            new IntersectionObserver(function (entries) {
                entries.forEach(function (entry) {
                    if (entry.isIntersecting && !userPaused) {
                        var p = video.play();
                        if (p && p.catch) p.catch(function () {});
                    } else if (!entry.isIntersecting) {
                        video.pause();
                    }
                });
            }, { threshold: 0.4 }).observe(video);
        });
    }

    /* --- Apariție la scroll ------------------------------------------- */

    function initReveal() {
        var els = document.querySelectorAll('.reveal');
        if (!('IntersectionObserver' in window)) {
            els.forEach(function (el) { el.classList.add('is-visible'); });
            return;
        }
        var observer = new IntersectionObserver(function (entries) {
            entries.forEach(function (entry) {
                if (entry.isIntersecting) {
                    entry.target.classList.add('is-visible');
                    observer.unobserve(entry.target);
                }
            });
        }, { rootMargin: '0px 0px -8% 0px', threshold: 0.05 });
        els.forEach(function (el) { observer.observe(el); });
    }

    /* --- Footer: coloanele se pliază pe mobil ------------------------- */

    function initFooter() {
        var details = document.querySelectorAll('.footer__details');
        function apply() {
            details.forEach(function (d) { d.open = !mobileQuery.matches; });
        }
        apply();
        mobileQuery.addEventListener('change', apply);
    }

    initAgeGate();
    initGlobalNav();
    initLocalNav();
    document.querySelectorAll('[data-gallery]').forEach(initGallery);
    initTabs();
    initVideos();
    initReveal();
    initFooter();
})();
