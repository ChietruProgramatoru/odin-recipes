# PetrutzCraft (Python version)

The original Python + pyglet version of the game, kept here until the C++ version in the
repository root has fully replaced it. It is no longer developed.

```bash
cd python-legacy
python -m venv .venv
# Windows:      .venv\Scripts\activate
# macOS/Linux:  source .venv/bin/activate
pip install -r requirements.txt
python main.py --seed 42
```

Tests: `python tests/test_logic.py`, and the headless smoke test
`xvfb-run -a -s "-screen 0 1280x720x24" python main.py --seed 7 --autotest OUT_DIR`.
