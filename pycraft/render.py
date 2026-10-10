"""OpenGL rendering with pyglet: chunk meshes, cows, dropped items and the block outline."""
import math
import time

import pyglet
from pyglet import gl
from pyglet.graphics.shader import Shader, ShaderProgram
from pyglet.math import Mat4, Vec3

from .blocks import T_BLACK
from .textures import tile_uv
from .mobs import build_models
from .world import CHUNK, MeshData

IDENTITY = Mat4()


def _mat4(m):
    """3x4 row-major affine matrix -> pyglet Mat4 (column-major)."""
    return Mat4(m[0][0], m[1][0], m[2][0], 0, m[0][1], m[1][1], m[2][1], 0,
                 m[0][2], m[1][2], m[2][2], 0, m[0][3], m[1][3], m[2][3], 1)

VERTEX_SRC = '''#version 330 core
in vec3 position;
in float shade;
in vec2 tex;
uniform mat4 mvp;
uniform mat4 model;
uniform vec3 eye;
out vec2 v_tex;
out float v_shade;
out float v_dist;
void main() {
    vec4 world = model * vec4(position, 1.0);
    gl_Position = mvp * world;
    v_tex = tex;
    v_shade = shade;
    v_dist = length(world.xyz - eye);
}
'''

FRAGMENT_SRC = '''#version 330 core
in vec2 v_tex;
in float v_shade;
in float v_dist;
uniform sampler2D atlas;
uniform vec4 fog_color;
uniform float fog_start;
uniform float fog_end;
uniform float alpha_cut;
out vec4 out_color;
void main() {
    vec4 c = texture(atlas, v_tex);
    if (c.a < alpha_cut) discard;
    c.rgb *= v_shade;
    float t = clamp((v_dist - fog_start) / (fog_end - fog_start), 0.0, 1.0);
    c.rgb = mix(c.rgb, fog_color.rgb, t);
    out_color = c;
}
'''


def pixel_texture(pil_image):
    """Upload a PIL image as a pixel-crisp texture (v = 1 is the top row of the image)."""
    img = pil_image.convert('RGBA')
    data = pyglet.image.ImageData(img.width, img.height, 'RGBA', img.tobytes(), pitch=-img.width * 4)
    tex = data.get_texture()
    gl.glBindTexture(tex.target, tex.id)
    gl.glTexParameteri(tex.target, gl.GL_TEXTURE_MIN_FILTER, gl.GL_NEAREST)
    gl.glTexParameteri(tex.target, gl.GL_TEXTURE_MAG_FILTER, gl.GL_NEAREST)
    return tex


class WorldRenderer:
    def __init__(self, world, atlas, render_distance=6, sky=(0.62, 0.8, 1.0, 1.0)):
        self.world = world
        self.atlas = atlas
        self.render_distance = render_distance
        self.sky = sky
        self.program = ShaderProgram(Shader(VERTEX_SRC, 'vertex'), Shader(FRAGMENT_SRC, 'fragment'))
        self.meshes = {}            # (cx, cz) -> (solid vertex list or None, water vertex list or None)
        self.dirty = set()
        self.on_chunk_shown = None  # callback(chunk) the first time a chunk is meshed
        self.models = {name: self._upload(mesh) for name, mesh in build_models(MeshData).items()}
        self.instances = []         # (model name, 3x4 matrix) for cows and dropped items this frame
        self.outline = None

    # --- chunk streaming ------------------------------------------------------
    def _upload(self, mesh):
        if not mesh.count:
            return None
        return self.program.vertex_list_indexed(
            mesh.count, gl.GL_TRIANGLES, mesh.indices,
            position=('f', mesh.positions), shade=('f', mesh.shades), tex=('f', mesh.uvs))

    def rebuild(self, key):
        chunk = self.world.chunk(*key)
        solid, water = self.world.build_mesh_data(chunk)
        old = self.meshes.pop(key, None)
        self.meshes[key] = (self._upload(solid), self._upload(water))
        if old is not None:
            for vl in old:
                if vl is not None:
                    vl.delete()
        elif self.on_chunk_shown:
            self.on_chunk_shown(chunk)

    def mark_dirty(self, keys):
        for k in keys:
            if k in self.meshes:
                self.dirty.add(k)

    def flush_dirty(self):
        for k in list(self.dirty):
            self.rebuild(k)
        self.dirty.clear()

    def wanted(self, px, pz, radius=None):
        pcx, pcz = math.floor(px) // CHUNK, math.floor(pz) // CHUNK
        r = self.render_distance if radius is None else radius
        keys = [(pcx + dx, pcz + dz) for dx in range(-r, r + 1) for dz in range(-r, r + 1)
                if dx * dx + dz * dz <= r * r + 1]
        keys.sort(key=lambda k: (k[0] - pcx) ** 2 + (k[1] - pcz) ** 2)
        return pcx, pcz, keys

    def update(self, px, pz, budget=0.008):
        """Mesh missing nearby chunks within a time budget and drop far ones."""
        self.flush_dirty()
        pcx, pcz, keys = self.wanted(px, pz)
        start = time.perf_counter()
        for k in keys:
            if k not in self.meshes:
                self.rebuild(k)
                if time.perf_counter() - start > budget:
                    break
        limit = (self.render_distance + 2) ** 2
        for k in [k for k in self.meshes if (k[0] - pcx) ** 2 + (k[1] - pcz) ** 2 > limit]:
            for vl in self.meshes.pop(k):
                if vl is not None:
                    vl.delete()

    def load_all(self, px, pz, radius=None):
        for k in self.wanted(px, pz, radius)[2]:
            if k not in self.meshes:
                self.rebuild(k)

    # --- per-frame geometry ----------------------------------------------------
    def set_instances(self, instances):
        self.instances = instances

    def set_outline(self, block):
        if self.outline is not None:
            self.outline.delete()
            self.outline = None
        if block is None:
            return
        x, y, z = block
        e = 0.003
        corners = [(x - e if i & 1 == 0 else x + 1 + e, y - e if i & 2 == 0 else y + 1 + e,
                    z - e if i & 4 == 0 else z + 1 + e) for i in range(8)]
        edges = [(0, 1), (2, 3), (4, 5), (6, 7), (0, 2), (1, 3), (4, 6), (5, 7), (0, 4), (1, 5), (2, 6), (3, 7)]
        u0, v0, u1, v1 = tile_uv(T_BLACK)
        pos = [c for a, b in edges for c in corners[a] + corners[b]]
        n = len(edges) * 2
        self.outline = self.program.vertex_list(n, gl.GL_LINES, position=('f', pos), shade=('f', [1.0] * n),
                                                tex=('f', [(u0 + u1) / 2, (v0 + v1) / 2] * n))

    # --- drawing --------------------------------------------------------------
    def draw(self, width, height, eye, forward):
        far = self.render_distance * CHUNK
        proj = Mat4.perspective_projection(width / max(1, height), z_near=0.05, z_far=far + 40, fov=70)
        target = Vec3(eye[0] + forward[0], eye[1] + forward[1], eye[2] + forward[2])
        view = Mat4.look_at(Vec3(*eye), target, Vec3(0, 1, 0))
        p = self.program
        p.use()
        p['mvp'] = proj @ view
        p['eye'] = tuple(eye)
        p['fog_color'] = self.sky
        p['fog_start'] = far * 0.55
        p['fog_end'] = far - 2
        p['atlas'] = 0
        p['alpha_cut'] = 0.5
        p['model'] = IDENTITY
        gl.glActiveTexture(gl.GL_TEXTURE0)
        gl.glBindTexture(self.atlas.target, self.atlas.id)

        gl.glEnable(gl.GL_DEPTH_TEST)
        gl.glEnable(gl.GL_CULL_FACE)
        gl.glFrontFace(gl.GL_CW)     # face corners are listed clockwise when seen from outside
        gl.glDisable(gl.GL_BLEND)
        for solid, _ in self.meshes.values():
            if solid is not None:
                solid.draw(gl.GL_TRIANGLES)
        for name, m in self.instances:
            p['model'] = _mat4(m)
            self.models[name].draw(gl.GL_TRIANGLES)
        p['model'] = IDENTITY
        if self.outline is not None:
            self.outline.draw(gl.GL_LINES)

        # Water: translucent, both sides visible, drawn last without writing depth.
        p['alpha_cut'] = 0.0
        gl.glDisable(gl.GL_CULL_FACE)
        gl.glEnable(gl.GL_BLEND)
        gl.glBlendFunc(gl.GL_SRC_ALPHA, gl.GL_ONE_MINUS_SRC_ALPHA)
        gl.glDepthMask(gl.GL_FALSE)
        for _, water in self.meshes.values():
            if water is not None:
                water.draw(gl.GL_TRIANGLES)
        gl.glDepthMask(gl.GL_TRUE)
        p.stop()

        # Leave the state the 2D HUD expects.
        gl.glDisable(gl.GL_DEPTH_TEST)
        gl.glFrontFace(gl.GL_CCW)
