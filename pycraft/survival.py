"""Inventory and health/hunger rules. Pure logic, no graphics."""
from .blocks import FOODS, LOG, PLANKS

STACK = 64
MAX_HEALTH = 20      # 10 hearts
MAX_HUNGER = 20      # 10 drumsticks

# Exhaustion costs (4.0 exhaustion = 1 hunger point once saturation is gone).
EXHAUST_WALK = 0.02      # per block walked
EXHAUST_SPRINT = 0.12    # per block sprinted
EXHAUST_JUMP = 0.06
EXHAUST_SPRINT_JUMP = 0.2
EXHAUST_BREAK = 0.03
EXHAUST_ATTACK = 0.1
EXHAUST_DAMAGE = 0.1
EXHAUST_REGEN = 3.0

REGEN_INTERVAL = 4.0     # seconds per half-heart healed when well fed
STARVE_INTERVAL = 4.0    # seconds per half-heart lost when starving


class Inventory:
    def __init__(self, size=9):
        self.slots = [None] * size   # each slot: [item_id, count] or None
        self.selected = 0

    def add(self, item, count=1):
        """Add items, returns how many did not fit."""
        for slot in self.slots:
            if slot and slot[0] == item and slot[1] < STACK:
                moved = min(count, STACK - slot[1])
                slot[1] += moved
                count -= moved
                if not count:
                    return 0
        for i, slot in enumerate(self.slots):
            if slot is None:
                moved = min(count, STACK)
                self.slots[i] = [item, moved]
                count -= moved
                if not count:
                    return 0
        return count

    def count(self, item):
        return sum(s[1] for s in self.slots if s and s[0] == item)

    def remove(self, item, count=1):
        if self.count(item) < count:
            return False
        for i, slot in enumerate(self.slots):
            if slot and slot[0] == item:
                taken = min(count, slot[1])
                slot[1] -= taken
                count -= taken
                if slot[1] == 0:
                    self.slots[i] = None
                if not count:
                    break
        return True

    @property
    def selected_item(self):
        slot = self.slots[self.selected]
        return slot[0] if slot else None

    def take_selected(self):
        slot = self.slots[self.selected]
        if not slot:
            return None
        slot[1] -= 1
        if slot[1] == 0:
            self.slots[self.selected] = None
        return slot[0]

    def craft_planks(self):
        """1 log -> 4 planks."""
        if self.count(LOG) < 1:
            return False
        self.remove(LOG, 1)
        left = self.add(PLANKS, 4)
        if left:   # no room: undo
            self.remove(PLANKS, 4 - left)
            self.add(LOG, 1)
            return False
        return True


class Vitals:
    def __init__(self):
        self.reset()

    def reset(self):
        self.health = MAX_HEALTH
        self.hunger = MAX_HUNGER
        self.saturation = 5.0
        self.exhaustion = 0.0
        self.regen_timer = 0.0
        self.starve_timer = 0.0
        self.hurt_timer = 0.0     # invulnerability after being hit

    @property
    def dead(self):
        return self.health <= 0

    @property
    def can_sprint(self):
        return self.hunger > 6

    def exhaust(self, amount):
        self.exhaustion += amount
        while self.exhaustion >= 4.0:
            self.exhaustion -= 4.0
            if self.saturation > 0:
                self.saturation = max(0.0, self.saturation - 1)
            else:
                self.hunger = max(0, self.hunger - 1)

    def damage(self, amount, ignore_cooldown=False):
        """Returns True if damage was applied."""
        if amount <= 0 or self.dead or (self.hurt_timer > 0 and not ignore_cooldown):
            return False
        self.health = max(0, self.health - amount)
        self.hurt_timer = 0.5
        self.exhaust(EXHAUST_DAMAGE)
        return True

    def eat(self, item):
        if item not in FOODS or self.hunger >= MAX_HUNGER:
            return False
        food, sat = FOODS[item]
        self.hunger = min(MAX_HUNGER, self.hunger + food)
        self.saturation = min(float(self.hunger), self.saturation + sat)
        return True

    def tick(self, dt):
        """Advance timers. Returns damage taken this tick (for screen flash)."""
        self.hurt_timer = max(0.0, self.hurt_timer - dt)
        took = 0
        if self.dead:
            return 0
        if self.hunger >= 18 and self.health < MAX_HEALTH:
            self.regen_timer += dt
            if self.regen_timer >= REGEN_INTERVAL:
                self.regen_timer = 0.0
                self.health += 1
                self.exhaust(EXHAUST_REGEN)
        else:
            self.regen_timer = 0.0
        if self.hunger == 0:
            self.starve_timer += dt
            if self.starve_timer >= STARVE_INTERVAL:
                self.starve_timer = 0.0
                if self.health > 1:      # like Normal difficulty: starving won't kill you
                    self.health -= 1
                    took = 1
        else:
            self.starve_timer = 0.0
        return took


def fall_damage(distance):
    return max(0, int(distance - 3.0 + 0.5))
