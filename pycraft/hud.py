"""On-screen interface drawn with pyglet: crosshair, hotbar, hearts, hunger and overlays."""
import pyglet
from pyglet import shapes
from pyglet.graphics import Batch, Group
from pyglet.sprite import Sprite
from pyglet.text import Label

from .blocks import ITEM_NAMES
from .render import pixel_texture

HELP = (
    'WASD move   SPACE jump/swim   SHIFT sprint\n'
    'Left mouse: mine block / hit cow (hold to mine)\n'
    'Right mouse: place block / eat food\n'
    '1-9 or wheel: pick slot   F: craft 1 log -> 4 planks\n'
    'C: creative mode (double-tap SPACE to fly, SHIFT down)\n'
    'ESC: pause   H: hide this help'
)


class Hud:
    def __init__(self, window, item_icons, hud_icons):
        self.window = window
        self.item_tex = {k: pixel_texture(v) for k, v in item_icons.items()}
        self.icon_tex = {k: pixel_texture(v) for k, v in hud_icons.items()}
        self.message_timer = 0.0
        self.item_name_timer = 0.0
        self.damage_alpha = 0.0
        self.show_help = True
        self._layout()

    def _layout(self):
        w, h = self.window.width, self.window.height
        self.batch = batch = Batch()
        back, mid, front, top = Group(0), Group(1), Group(2), Group(3)
        self.slot = slot = max(36, min(64, int(min(w / 14, h / 9))))
        x0 = w / 2 - slot * 4.5
        y0 = 8

        self.water = shapes.Rectangle(0, 0, w, h, color=(25, 64, 180, 110), batch=batch, group=back)
        self.damage = shapes.Rectangle(0, 0, w, h, color=(200, 0, 0, 0), batch=batch, group=back)
        c = (255, 255, 255, 220)
        self.cross = [shapes.Rectangle(w / 2 - 10, h / 2 - 1, 20, 2, color=c, batch=batch, group=mid),
                      shapes.Rectangle(w / 2 - 1, h / 2 - 10, 2, 20, color=c, batch=batch, group=mid)]

        self.selector = shapes.Rectangle(x0, y0, slot, slot, color=(255, 255, 255, 240), batch=batch, group=mid)
        self.slots, self.icons, self.counts = [], [], []
        for i in range(9):
            sx = x0 + i * slot
            self.slots.append(shapes.Rectangle(sx + 3, y0 + 3, slot - 6, slot - 6, color=(25, 25, 25, 200),
                                               batch=batch, group=front))
            icon = Sprite(next(iter(self.item_tex.values())), x=sx + slot * 0.18, y=y0 + slot * 0.18,
                          batch=batch, group=top)
            icon.scale = slot * 0.64 / icon.width
            icon.visible = False
            self.icons.append(icon)
            self.counts.append(Label('', font_size=max(9, slot // 5), x=sx + slot - 5, y=y0 + 5,
                                     anchor_x='right', anchor_y='bottom', batch=batch, group=top))

        size = slot * 0.42
        row_y = y0 + slot + 6
        self.hearts, self.food = [], []
        for i in range(10):
            hs = Sprite(self.icon_tex['heart_full'], x=x0 + i * size * 1.05, y=row_y, batch=batch, group=top)
            hs.scale = size / hs.width
            self.hearts.append(hs)
            fs = Sprite(self.icon_tex['food_full'], x=x0 + slot * 9 - (i + 1) * size * 1.05, y=row_y,
                        batch=batch, group=top)
            fs.scale = size / fs.width
            self.food.append(fs)

        self.mine_bg = shapes.Rectangle(w / 2 - 50, h / 2 - 34, 100, 8, color=(0, 0, 0, 150), batch=batch, group=mid)
        self.mine_fill = shapes.Rectangle(w / 2 - 48, h / 2 - 32, 0, 4, color=(255, 255, 255, 230),
                                          batch=batch, group=front)

        self.item_name = Label('', font_size=14, x=w / 2, y=row_y + size + 16, anchor_x='center', batch=batch,
                               group=top)
        self.message = Label('', font_size=18, x=w / 2, y=h * 0.68, anchor_x='center', batch=batch, group=top)
        self.info = Label('', font_size=12, x=10, y=h - 10, anchor_y='top', batch=batch, group=top)
        self.help = Label(HELP, font_size=11, x=10, y=h - 36, anchor_y='top', multiline=True, width=520,
                          batch=batch, group=top)

        self.death_bg = shapes.Rectangle(0, 0, w, h, color=(115, 0, 0, 150), batch=batch, group=top)
        self.death_text = Label('You died!\n\nPress R to respawn', font_size=30, x=w / 2, y=h / 2,
                                anchor_x='center', anchor_y='center', align='center', multiline=True, width=w,
                                batch=batch, group=top)
        self.pause_text = Label('Paused\nClick to resume', font_size=26, x=w / 2, y=h / 2, anchor_x='center',
                                anchor_y='center', align='center', multiline=True, width=w, batch=batch,
                                group=top)

    def resize(self):
        self._layout()

    # --- events ----------------------------------------------------------------
    def flash_damage(self):
        self.damage_alpha = 0.45

    def show_message(self, text, seconds=2.5):
        self.message.text = text
        self.message_timer = seconds

    def show_item_name(self, item):
        self.item_name.text = ITEM_NAMES.get(item, '') if item is not None else ''
        self.item_name_timer = 2.0

    def set_mining(self, progress):
        on = progress is not None
        self.mine_bg.visible = self.mine_fill.visible = on
        if on:
            self.mine_fill.width = 96 * min(1.0, progress)

    # --- per frame ---------------------------------------------------------------
    def update(self, dt, inventory, vitals, creative, info_text, underwater, dead, paused):
        x0 = self.window.width / 2 - self.slot * 4.5
        self.selector.x = x0 + inventory.selected * self.slot
        for i, slot in enumerate(inventory.slots):
            icon, count = self.icons[i], self.counts[i]
            if slot:
                if icon.image is not self.item_tex[slot[0]]:
                    icon.image = self.item_tex[slot[0]]
                icon.visible = True
                count.text = '' if creative or slot[1] == 1 else str(slot[1])
            else:
                icon.visible = False
                count.text = ''

        for i in range(10):
            hp = vitals.health - i * 2
            key = 'heart_full' if hp >= 2 else 'heart_half' if hp == 1 else 'heart_empty'
            if self.hearts[i].image is not self.icon_tex[key]:
                self.hearts[i].image = self.icon_tex[key]
            fd = vitals.hunger - i * 2
            key = 'food_full' if fd >= 2 else 'food_half' if fd == 1 else 'food_empty'
            if self.food[i].image is not self.icon_tex[key]:
                self.food[i].image = self.icon_tex[key]
            self.hearts[i].visible = self.food[i].visible = not creative

        self.item_name_timer -= dt
        self.item_name.visible = self.item_name_timer > 0
        self.message_timer -= dt
        self.message.visible = self.message_timer > 0
        self.info.text = info_text
        self.help.visible = self.show_help
        self.water.visible = underwater
        self.damage_alpha = max(0.0, self.damage_alpha - dt * 1.2)
        self.damage.opacity = int(255 * self.damage_alpha)
        self.death_bg.visible = self.death_text.visible = dead
        self.pause_text.visible = paused and not dead

    def draw(self):
        pyglet.gl.glEnable(pyglet.gl.GL_BLEND)
        pyglet.gl.glBlendFunc(pyglet.gl.GL_SRC_ALPHA, pyglet.gl.GL_ONE_MINUS_SRC_ALPHA)
        self.batch.draw()
