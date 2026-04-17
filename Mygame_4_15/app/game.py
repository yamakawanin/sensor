import base64
import io
import random
import sys
import time
from dataclasses import dataclass
from typing import Optional

import pygame

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    serial = None
    list_ports = None


WIDTH = 600
HEIGHT = 150
FPS = 60

ACCELERATION = 0.0005
BG_CLOUD_SPEED = 0.2
BOTTOM_PAD = 10
GAP_COEFFICIENT = 0.85
GRAVITY = 0.6
INITIAL_JUMP_VELOCITY = -10
MAX_OBSTACLE_LENGTH = 2
MAX_SPEED = 9.5
SPEED = 4.6

DARK_ON_THRESHOLD = 320
DARK_OFF_THRESHOLD = 380


TREX_B64 = "iVBORw0KGgoAAAANSUhEUgAAAQgAAAAvAgMAAABiRrxWAAAADFBMVEX///9TU1P39/f///+TS9URAAAAAXRSTlMAQObYZgAAAPpJREFUeF7d0jFKRkEMhdGLMM307itNLALyVmHvJuzTDMjdn72E95PGFEZSmeoU4YMMgxhskvQec8YSVFX1NhGcS5ywtbmC8khcZeKq+ZWJ4F8Sr2+ZCErjkJFEfcjAc/6/BMlfcz6xHdhRthYzIZhIHMcTVY1scUUiAphK8CMSPUbieTBhvD9Lj0vyV4wklEGzHpciKGOJoBp7XDcFs4kWxxM7Ey3iZ8JbzASAvMS7XLOJHTTvEkEZSeQl7DMuwVyCasqK5+XzQRYLUJlMbPXjFcn3m8eKBSjWZMJwvGIOvViAzCbUj1VEDoqFOEQGE3SyInJQLOQMJL4B7enP1UbLXJQAAAAASUVORK5CYII="
OBSTACLE_SMALL_B64 = "iVBORw0KGgoAAAANSUhEUgAAAGYAAAAjCAMAAABRlI+PAAAADFBMVEX////////39/dTU1PhglcSAAAAAXRSTlMAQObYZgAAAPNJREFUeF7tlkEKwzAMBLXr//+5iQhU7gRRQkyhZI+DhwH74jhmO+oIJBVwURljuAXagG5QqkSgBLqg3JnxJ1Cb8SmQ3o6gpO85owGlOB4m2BNKJ11BSd01owGlOHkcIAuHkz6UNpPKgozPM54dADHjJuNhZiJxdQCQgZJeBczgCAAy3yhPJvcnmdC9mZwBIsQMFV5AkzHBNknFgcKM+oyDIFcfCAoy03m+jSMIcmoVZkKqSjr1fghyahRmoKRUHYLiSI1SMlCq5CDgX6BXmKkfn+oQ0KEyyrzoy8GbXJ9xrM/YjhUZgl9nnsyTCe9rgSRdV15CwRcIEu8GGQAAAABJRU5ErkJggg=="
OBSTACLE_LARGE_B64 = "iVBORw0KGgoAAAANSUhEUgAAAJYAAAAyCAMAAACJUtIoAAAACVBMVEX////39/dTU1OabbyfAAAAAXRSTlMAQObYZgAAAXhJREFUeF7t2NGqAjEMANGM///RlwvaYQndULuFPJgHUYaEI6IPhgNAOA8HZ+3U6384F5y1U6YzAZTWG+dZamnFEstBFtCKJZSHWMADLJ18z+JqpQeLdKoDC8siC5iFCQs4znIxB5B1t6F3lQWkL4N0JsF+u6GXJdbI+FKW+yWr3lhgCZ2VSag3Nlk/FnRkIRbasLCO0oulikMsvmGpeiGLZ1jOMgtIP5bODivYYUXEIVbwFCt4khVssRgsgidZwQaLd2A8m7MYLGTl4KeQQs2y4kMAMGGlmQViDIb5O6xZnnLD485dIBzqDSE1yyFdL4Iqu4XJqUUWl/NVAFSZq1P6a5aqbAUM2epQbBioWflUBABiUyhYyZoCBev8XyMAObDNOhOAfiyxmHU0YNlldGAphGjFCjA3YkUn1o/1Y3EkZFZ5isCC6NUgwDBn1RuXH96doNfAhDXfsIyJ2AnolcCVhay0kcYbW0HvCO8OwIcJ3GzkORpkFuUP/1Ec8FW1qJkAAAAASUVORK5CYII="
CLOUD_B64 = "iVBORw0KGgoAAAANSUhEUgAAAC4AAAAOCAQAAAD6HOaKAAAAU0lEQVR4XrWSsQkAQAgD3X9El/ELixQpJHCfdApnUCtXz7o49cgagaGPaq4rIwAP9s/C7R7UX3inJ0BDb6qWDC7ScOR/QWjRlFizuPwLtTLj+qkH6DjD2wLtikUAAAAASUVORK5CYII="
HORIZON_B64 = "iVBORw0KGgoAAAANSUhEUgAABLAAAAAMAgMAAAAPCKxBAAAABlBMVEX///9TU1NYzE1OAAAAAXRSTlMAQObYZgAAALJJREFUeF7t1EEKAyEMhtEvMNm7sPfJEVyY+1+ltLgYAsrQCtWhbxEhQvgxIJtSZypxa/WGshgzKdbq/UihMFMlt3o/CspEYoihIMaAb6mCvM6C+BTAeyo+wN4yykV/6pVfkdLpVyI1hh7GJ6QunUoLEQlQglNP2nkQkeF8+ei9cLxMue1qxVRfk1Ej0s6AEGWfVOk0QUtnK5Xo0Lac6wpdtnQqB6VxomPaz+dgF1PaqqmeWJlz1jYUaSIAAAAASUVORK5CYII="
TEXT_B64 = "iVBORw0KGgoAAAANSUhEUgAAAL8AAAAYAgMAAADWncTDAAAABlBMVEX///9TU1NYzE1OAAAAAXRSTlMAQObYZgAAAO1JREFUeF690TFqxUAQA1BNoRtk7jMu3E9Auv9Vgr/5A863Y9zEhVhkHmhZsEGkw4Lppmllh1tcLHx+aRj2YnEDuQFvcQW+EoZY0TQLCZbEVxRxAvY+i8ikW0C0bwFdbictG2zvu/4EcCuBF0B23IBsQHZBYgm1n86BN+BmyV5rQFyCJAiDJSTfgBV9BbjvXdzIcKchpMOYd3gO/jvCeuUGFALg95J0/SrtQlrzz+sAjDwCIQsbWAdgbqrQpKYRjmPuAfU5dMC+c0rxOTiO+T6ZlK4pbcDLI1DIRaf3GxDGALkQHnD+cGhMKeox+AEOL3mLO7TQZgAAAABJRU5ErkJggg=="


def decode_png(b64: str) -> pygame.Surface:
    raw = base64.b64decode(b64)
    return pygame.image.load(io.BytesIO(raw)).convert_alpha()


def make_tinted(surface: pygame.Surface, color) -> pygame.Surface:
    tinted = pygame.Surface(surface.get_size(), pygame.SRCALPHA)
    tinted.fill(color)
    alpha = surface.copy()
    tinted.blit(alpha, (0, 0), special_flags=pygame.BLEND_RGBA_MULT)
    return tinted


@dataclass
class InputState:
    jump_pressed: bool = False
    distance_cm: Optional[int] = None
    light_value: Optional[int] = None
    jump_signal: Optional[int] = None


class ArduinoInput:
    def __init__(self, baudrate: int = 115200, timeout: float = 0.0):
        self.baudrate = baudrate
        self.timeout = timeout
        self.ser = None
        self.port = None
        self.jump_cooldown_until = 0.0
        self.last_distance = None
        self.last_light = None
        self.last_jump_signal = 0

    @property
    def connected(self) -> bool:
        return self.ser is not None and self.ser.is_open

    def connect(self) -> bool:
        if serial is None or list_ports is None:
            return False

        preferred = []
        others = []
        for p in list_ports.comports():
            text = f"{p.device} {p.description}".lower()
            if any(k in text for k in ["arduino", "usb", "wch", "cp210", "tty.usb", "ttyacm"]):
                preferred.append(p.device)
            else:
                others.append(p.device)

        for dev in preferred + others:
            try:
                ser_obj = serial.Serial(dev, self.baudrate, timeout=self.timeout)
                time.sleep(1.2)
                ser_obj.reset_input_buffer()
                self.ser = ser_obj
                self.port = dev
                return True
            except Exception:
                continue
        return False

    def close(self):
        if self.ser is not None:
            try:
                self.ser.close()
            except Exception:
                pass
        self.ser = None

    def poll(self) -> InputState:
        state = InputState()
        if not self.connected:
            return state

        latest = None
        try:
            while self.ser.in_waiting > 0:
                line = self.ser.readline().decode("utf-8", errors="ignore")
                parts = line.strip().split(",")
                if len(parts) in (2, 3):
                    try:
                        distance = int(parts[0])
                        light = int(parts[1])
                        jump_signal = int(parts[2]) if len(parts) == 3 else 0
                        latest = (distance, light, jump_signal)
                    except ValueError:
                        pass
        except Exception:
            self.close()
            return state

        if latest is None:
            state.distance_cm = self.last_distance
            state.light_value = self.last_light
            state.jump_signal = self.last_jump_signal
            return state

        distance, light, jump_signal = latest
        self.last_distance = distance
        self.last_light = light
        self.last_jump_signal = jump_signal
        state.distance_cm = distance
        state.light_value = light
        state.jump_signal = jump_signal

        now = time.time()
        if jump_signal == 1 and now >= self.jump_cooldown_until:
            state.jump_pressed = True
            self.jump_cooldown_until = now + 0.34
        elif 3 <= distance <= 200 and distance < 22 and now >= self.jump_cooldown_until:
            state.jump_pressed = True
            self.jump_cooldown_until = now + 0.34

        return state


class SpriteBank:
    def __init__(self, color):
        self.trex_sheet = make_tinted(decode_png(TREX_B64), color)
        self.ob_small = make_tinted(decode_png(OBSTACLE_SMALL_B64), color)
        self.ob_large = make_tinted(decode_png(OBSTACLE_LARGE_B64), color)
        self.cloud = make_tinted(decode_png(CLOUD_B64), color)
        self.horizon = make_tinted(decode_png(HORIZON_B64), color)
        self.text = make_tinted(decode_png(TEXT_B64), color)


class Trex:
    WIDTH = 44
    HEIGHT = 47
    START_X_POS = 50
    MIN_JUMP_HEIGHT = 30
    MAX_JUMP_HEIGHT = 30
    DROP_VELOCITY = -5
    BLINK_TIMING = 7000

    COLLISION_BOXES = [
        pygame.Rect(1, -1, 30, 26),
        pygame.Rect(32, 0, 8, 16),
        pygame.Rect(10, 35, 14, 8),
        pygame.Rect(1, 24, 29, 5),
        pygame.Rect(5, 30, 21, 4),
        pygame.Rect(9, 34, 15, 4),
    ]

    def __init__(self):
        self.x = Trex.START_X_POS
        self.ground_y = HEIGHT - Trex.HEIGHT - BOTTOM_PAD
        self.y = self.ground_y

        self.status = "WAITING"
        self.jumping = False
        self.jump_velocity = 0.0
        self.speed_drop = False
        self.reached_min_height = False

        self.anim_frames = {
            "WAITING": ([44, 0], 1000 / 3),
            "RUNNING": ([88, 132], 1000 / 12),
            "JUMPING": ([0], 1000 / 60),
            "CRASHED": ([220], 1000 / 60),
        }

        self.current_frame = 0
        self.timer = 0.0
        self.blink_delay = random.randint(1, Trex.BLINK_TIMING)
        self.anim_start = pygame.time.get_ticks()

    @property
    def rect(self):
        return pygame.Rect(int(self.x), int(self.y), Trex.WIDTH, Trex.HEIGHT)

    def set_status(self, status: str):
        self.status = status
        self.current_frame = 0
        self.timer = 0.0
        if status == "WAITING":
            self.anim_start = pygame.time.get_ticks()
            self.blink_delay = random.randint(1, Trex.BLINK_TIMING)

    def start_jump(self):
        if not self.jumping:
            self.set_status("JUMPING")
            self.jumping = True
            self.jump_velocity = INITIAL_JUMP_VELOCITY
            self.reached_min_height = False
            self.speed_drop = False

    def end_jump(self):
        if self.reached_min_height and self.jump_velocity < Trex.DROP_VELOCITY:
            self.jump_velocity = Trex.DROP_VELOCITY

    def update_jump(self, dt_ms: float):
        frames_elapsed = dt_ms / (1000 / FPS)
        if self.speed_drop:
            self.y += round(self.jump_velocity * 3 * frames_elapsed)
        else:
            self.y += round(self.jump_velocity * frames_elapsed)

        self.jump_velocity += GRAVITY * frames_elapsed
        min_jump_y = self.ground_y - Trex.MIN_JUMP_HEIGHT

        if self.y < min_jump_y or self.speed_drop:
            self.reached_min_height = True
        if self.y < Trex.MAX_JUMP_HEIGHT or self.speed_drop:
            self.end_jump()

        if self.y > self.ground_y:
            self.reset()

    def reset(self):
        self.y = self.ground_y
        self.jump_velocity = 0.0
        self.jumping = False
        self.speed_drop = False
        self.set_status("RUNNING")

    def update(self, dt_ms: float):
        self.timer += dt_ms
        frames, ms_per_frame = self.anim_frames[self.status]

        if self.status == "WAITING":
            now = pygame.time.get_ticks()
            if now - self.anim_start >= self.blink_delay:
                if self.timer >= ms_per_frame:
                    self.current_frame = (self.current_frame + 1) % len(frames)
                    self.timer = 0.0
                    if self.current_frame == 1:
                        self.blink_delay = random.randint(1, Trex.BLINK_TIMING)
                        self.anim_start = now
        else:
            if self.timer >= ms_per_frame:
                self.current_frame = (self.current_frame + 1) % len(frames)
                self.timer = 0.0

    def draw(self, screen: pygame.Surface, bank: SpriteBank):
        frames, _ = self.anim_frames[self.status]
        src_x = frames[self.current_frame]
        src = pygame.Rect(src_x, 0, Trex.WIDTH, Trex.HEIGHT)
        screen.blit(bank.trex_sheet, (int(self.x), int(self.y)), src)


class Obstacle:
    TYPES = [
        {
            "type": "CACTUS_SMALL",
            "width": 17,
            "height": 35,
            "y": 105,
            "multiple_speed": 3,
            "min_gap": 120,
            "boxes": [
                pygame.Rect(0, 7, 5, 27),
                pygame.Rect(4, 0, 6, 34),
                pygame.Rect(10, 4, 7, 14),
            ],
        },
        {
            "type": "CACTUS_LARGE",
            "width": 25,
            "height": 50,
            "y": 90,
            "multiple_speed": 6,
            "min_gap": 120,
            "boxes": [
                pygame.Rect(0, 12, 7, 38),
                pygame.Rect(8, 0, 7, 49),
                pygame.Rect(13, 10, 10, 38),
            ],
        },
    ]

    def __init__(self, kind, speed):
        self.kind = kind
        self.speed = speed
        self.size = random.randint(1, MAX_OBSTACLE_LENGTH)
        if self.size > 1 and kind["multiple_speed"] > speed:
            self.size = 1

        self.width = kind["width"] * self.size
        self.height = kind["height"]
        self.x = WIDTH - self.width
        self.y = kind["y"]

        self.boxes = [pygame.Rect(b.x, b.y, b.w, b.h) for b in kind["boxes"]]
        if self.size > 1:
            self.boxes[1].width = self.width - self.boxes[0].width - self.boxes[2].width
            self.boxes[2].x = self.width - self.boxes[2].width

        min_gap = round(self.width * speed + kind["min_gap"] * GAP_COEFFICIENT)
        max_gap = round(min_gap * 1.5)
        self.gap = random.randint(min_gap, max_gap)

    @property
    def rect(self):
        return pygame.Rect(int(self.x), self.y, self.width, self.height)

    def update(self, dt_ms, speed):
        self.x -= int((speed * FPS / 1000) * dt_ms)

    def visible(self):
        return self.x + self.width > 0

    def draw(self, screen: pygame.Surface, bank: SpriteBank):
        if self.kind["type"] == "CACTUS_SMALL":
            sheet = bank.ob_small
            source_w = 17
            source_h = 35
        else:
            sheet = bank.ob_large
            source_w = 25
            source_h = 50

        source_x = int((source_w * self.size) * (0.5 * (self.size - 1)))
        src = pygame.Rect(source_x, 0, source_w * self.size, source_h)
        screen.blit(sheet, (int(self.x), self.y), src)


class Cloud:
    WIDTH = 46
    HEIGHT = 14

    def __init__(self):
        self.x = WIDTH
        self.y = random.randint(30, 71)
        self.gap = random.randint(100, 400)

    def update(self, speed):
        self.x -= int(speed)

    def visible(self):
        return self.x + Cloud.WIDTH > 0

    def draw(self, screen: pygame.Surface, bank: SpriteBank):
        src = pygame.Rect(0, 0, Cloud.WIDTH, Cloud.HEIGHT)
        screen.blit(bank.cloud, (int(self.x), self.y), src)


class HorizonLine:
    Y = 127
    W = 600
    H = 12

    def __init__(self):
        self.x = [0, HorizonLine.W]
        self.source_x = [0, HorizonLine.W]
        self.bump_threshold = 0.5

    def rand_type(self):
        return HorizonLine.W if random.random() > self.bump_threshold else 0

    def update(self, dt_ms: float, speed: float):
        inc = int(speed * (FPS / 1000) * dt_ms)
        idx = 0 if self.x[0] <= 0 else 1
        other = 1 - idx
        self.x[idx] -= inc
        self.x[other] = self.x[idx] + HorizonLine.W

        if self.x[idx] <= -HorizonLine.W:
            self.x[idx] += HorizonLine.W * 2
            self.x[other] = self.x[idx] - HorizonLine.W
            self.source_x[idx] = self.rand_type()

    def draw(self, screen: pygame.Surface, bank: SpriteBank):
        src0 = pygame.Rect(self.source_x[0], 0, HorizonLine.W, HorizonLine.H)
        src1 = pygame.Rect(self.source_x[1], 0, HorizonLine.W, HorizonLine.H)
        screen.blit(bank.horizon, (self.x[0], HorizonLine.Y), src0)
        screen.blit(bank.horizon, (self.x[1], HorizonLine.Y), src1)


class DistanceMeter:
    DIGIT_W = 10
    DIGIT_H = 13
    DEST_W = 11

    MAX_UNITS = 5
    ACHIEVEMENT = 100
    COEFFICIENT = 0.025
    FLASH_DURATION = 1000 / 4
    FLASH_ITERATIONS = 3

    def __init__(self):
        self.x = WIDTH - (DistanceMeter.DEST_W * (DistanceMeter.MAX_UNITS + 1))
        self.y = 5
        self.digits = list("00000")
        self.high = []
        self.achieve = False
        self.flash_timer = 0.0
        self.flash_iter = 0

    def actual(self, distance_px):
        return round(distance_px * DistanceMeter.COEFFICIENT)

    def set_high(self, distance_px):
        s = str(self.actual(distance_px)).rjust(DistanceMeter.MAX_UNITS, "0")
        self.high = [10, 11, None] + [int(ch) for ch in s]

    def update(self, dt_ms: float, distance_px: float):
        paint = True
        score = self.actual(distance_px)

        if not self.achieve:
            if score > 0 and score % DistanceMeter.ACHIEVEMENT == 0:
                self.achieve = True
                self.flash_timer = 0
                self.flash_iter = 0
            self.digits = list(str(score).rjust(DistanceMeter.MAX_UNITS, "0"))
        else:
            if self.flash_iter <= DistanceMeter.FLASH_ITERATIONS:
                self.flash_timer += dt_ms
                if self.flash_timer < DistanceMeter.FLASH_DURATION:
                    paint = False
                elif self.flash_timer > DistanceMeter.FLASH_DURATION * 2:
                    self.flash_timer = 0
                    self.flash_iter += 1
            else:
                self.achieve = False
                self.flash_timer = 0
                self.flash_iter = 0

        return paint

    def draw_digit(self, screen: pygame.Surface, bank: SpriteBank, pos, value, high=False):
        sx = DistanceMeter.DIGIT_W * value
        src = pygame.Rect(sx, 0, DistanceMeter.DIGIT_W, DistanceMeter.DIGIT_H)
        tx = pos * DistanceMeter.DEST_W
        base_x = self.x - (DistanceMeter.MAX_UNITS * 2) * DistanceMeter.DIGIT_W if high else self.x
        screen.blit(bank.text, (base_x + tx, self.y), src)

    def draw(self, screen: pygame.Surface, bank: SpriteBank, paint=True):
        if paint:
            for i in range(len(self.digits) - 1, -1, -1):
                self.draw_digit(screen, bank, i, int(self.digits[i]), high=False)
        for i in range(len(self.high) - 1, -1, -1):
            val = self.high[i]
            if val is None:
                continue
            self.draw_digit(screen, bank, i, val, high=True)


class Game:
    def __init__(self):
        pygame.init()
        pygame.display.set_caption("Dinosaur Game")
        self.screen = pygame.display.set_mode((WIDTH, HEIGHT))
        self.clock = pygame.time.Clock()
        self.big_font = pygame.font.SysFont("Menlo", 20)

        self.day_bank = SpriteBank((83, 83, 83, 255))
        self.night_bank = SpriteBank((245, 245, 245, 255))

        self.arduino = ArduinoInput()
        self.has_arduino = self.arduino.connect()

        self.trex = Trex()
        self.horizon = HorizonLine()
        self.clouds = [Cloud()]
        self.obstacles = []

        self.distance_meter = DistanceMeter()
        self.distance_ran = 0.0
        self.best_distance = 0.0

        self.started = False
        self.crashed = False
        self.speed = SPEED

        self.night_mode = False

    def update_night_mode(self, sensor: InputState, key_toggle: bool):
        if self.has_arduino and sensor.light_value is not None:
            if sensor.light_value <= DARK_ON_THRESHOLD:
                self.night_mode = True
            elif sensor.light_value >= DARK_OFF_THRESHOLD:
                self.night_mode = False
        elif key_toggle:
            self.night_mode = not self.night_mode

    def reset_round(self):
        self.trex = Trex()
        self.horizon = HorizonLine()
        self.clouds = [Cloud()]
        self.obstacles = []
        self.distance_ran = 0.0
        self.speed = SPEED
        self.started = False
        self.crashed = False

    def handle_events(self):
        jump_pressed = False
        toggle_c = False

        for e in pygame.event.get():
            if e.type == pygame.QUIT:
                return False, jump_pressed
            if e.type == pygame.KEYDOWN:
                if e.key in (pygame.K_SPACE, pygame.K_UP):
                    jump_pressed = True
                if e.key == pygame.K_c:
                    toggle_c = True

        sensor = self.arduino.poll() if self.has_arduino else InputState()
        jump_pressed = jump_pressed or sensor.jump_pressed
        self.update_night_mode(sensor, toggle_c)

        return True, jump_pressed

    def spawn_obstacle_if_needed(self):
        # 给玩家起跑缓冲区，避免刚开始就立刻遇到障碍
        if self.distance_ran < 120:
            return

        if not self.obstacles:
            kind = random.choice(Obstacle.TYPES)
            self.obstacles.append(Obstacle(kind, self.speed))
            return

        last = self.obstacles[-1]
        if last.x + last.width + last.gap < WIDTH:
            kind = random.choice(Obstacle.TYPES)
            self.obstacles.append(Obstacle(kind, self.speed))

    def check_collision(self):
        trex_rect = self.trex.rect
        for obstacle in self.obstacles:
            if not trex_rect.colliderect(obstacle.rect):
                continue
            for tb in Trex.COLLISION_BOXES:
                t = pygame.Rect(trex_rect.x + tb.x, trex_rect.y + tb.y, tb.w, tb.h)
                for ob in obstacle.boxes:
                    o = pygame.Rect(int(obstacle.x) + ob.x, obstacle.y + ob.y, ob.w, ob.h)
                    if t.colliderect(o):
                        return True
        return False

    def update(self, dt_ms: float, jump_pressed: bool):
        if self.crashed:
            if jump_pressed:
                self.reset_round()
            return

        if not self.started:
            self.trex.set_status("WAITING")
            if jump_pressed:
                self.started = True
                self.trex.set_status("RUNNING")
                self.trex.start_jump()
        else:
            if jump_pressed and not self.trex.jumping:
                self.trex.start_jump()

            if self.trex.jumping:
                self.trex.update_jump(dt_ms)

            self.spawn_obstacle_if_needed()
            self.horizon.update(dt_ms, self.speed)

            for cloud in self.clouds:
                cloud.update(BG_CLOUD_SPEED)
            self.clouds = [c for c in self.clouds if c.visible()]
            if len(self.clouds) < 6 and random.random() < 0.005:
                self.clouds.append(Cloud())

            for obstacle in self.obstacles:
                obstacle.update(dt_ms, self.speed)
            self.obstacles = [o for o in self.obstacles if o.visible()]

            self.distance_ran += self.speed * dt_ms / (1000 / FPS)

            # 前期加速更柔和，跑得越远加速越接近正常值
            accel_scale = min(1.0, 0.18 + self.distance_ran / 1400)
            self.speed = min(MAX_SPEED, self.speed + ACCELERATION * accel_scale * dt_ms)

            if self.check_collision():
                self.crashed = True
                self.trex.set_status("CRASHED")
                self.best_distance = max(self.best_distance, self.distance_ran)
                self.distance_meter.set_high(self.best_distance)

        self.trex.update(dt_ms)

    def draw(self, dt_ms: float):
        if self.night_mode:
            bg = (32, 32, 32)
            bank = self.night_bank
        else:
            bg = (255, 255, 255)
            bank = self.day_bank

        self.screen.fill(bg)

        for cloud in self.clouds:
            cloud.draw(self.screen, bank)

        self.horizon.draw(self.screen, bank)

        for obstacle in self.obstacles:
            obstacle.draw(self.screen, bank)

        self.trex.draw(self.screen, bank)

        paint_score = self.distance_meter.update(dt_ms, self.distance_ran)
        self.distance_meter.draw(self.screen, bank, paint=paint_score)

        if self.crashed:
            fg = (245, 245, 245) if self.night_mode else (83, 83, 83)
            over = self.big_font.render("GAME OVER", True, fg)
            self.screen.blit(over, (WIDTH // 2 - over.get_width() // 2, 55))

        pygame.display.flip()

    def run(self):
        running = True
        while running:
            dt_ms = self.clock.tick(FPS)
            running, jump_pressed = self.handle_events()
            self.update(dt_ms, jump_pressed)
            self.draw(dt_ms)

        self.arduino.close()
        pygame.quit()


def main():
    Game().run()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pygame.quit()
        sys.exit(0)
