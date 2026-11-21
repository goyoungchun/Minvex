import { writeFileSync } from 'node:fs';

const part = (name, mesh, translation = [0, 0, 0]) => ({ name, mesh, translation });

function geometry(material, build) {
    const positions = [];
    const normals = [];
    const indices = [];
    function quad(a, b, c, d) {
        const u = b.map((value, axis) => value - a[axis]);
        const v = c.map((value, axis) => value - a[axis]);
        const normal = [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]];
        const length = Math.hypot(...normal);
        const first = positions.length / 3;
        positions.push(...a, ...b, ...c, ...d);
        for (let i = 0; i < 4; ++i) normals.push(...normal.map(value => value / length));
        indices.push(first, first + 1, first + 2, first, first + 2, first + 3);
    }
    function box(x, y, z, w, h, d) {
        const a = [x - w / 2, y - h / 2, z - d / 2], b = [x + w / 2, a[1], a[2]];
        const c = [b[0], y + h / 2, a[2]], e = [a[0], c[1], a[2]];
        const top = [a, b, c, e].map(point => [point[0], point[1], z + d / 2]);
        quad(...top);
        quad(e, c, b, a);
        const bottom = [a, b, c, e];
        for (let i = 0; i < 4; ++i) {
            const j = (i + 1) % 4;
            quad(bottom[i], bottom[j], top[j], top[i]);
        }
    }
    function ring(count, radiusAt, bore, depth, phase = 0) {
        const point = (i, radius, z) => {
            const angle = i * 2 * Math.PI / count + phase;
            return [radius * Math.cos(angle), radius * Math.sin(angle), z];
        };
        for (let i = 0; i < count; ++i) {
            const j = (i + 1) % count;
            const a = point(i, radiusAt(i), depth / 2), b = point(j, radiusAt(j), depth / 2);
            const c = point(j, bore, depth / 2), d = point(i, bore, depth / 2);
            const [a0, b0, c0, d0] = [a, b, c, d].map(point => [point[0], point[1], -depth / 2]);
            quad(a, b, c, d);
            quad(d0, c0, b0, a0);
            quad(a0, b0, b, a);
            quad(c0, d0, d, c);
        }
    }
    build(box, ring);
    return { material, positions, normals, indices };
}

function gearbox(alternate) {
    const colors = [
        alternate ? [.12, .30, .24] : [.11, .23, .36],
        alternate ? [.29, .42, .35] : [.24, .38, .52],
        [.65, .40, .15], [.43, .53, .60], [.51, .56, .63],
    ];
    const meshes = [
        geometry(0, box => {
            box(0, 0, -.38, 3.36, 2.24, .18);
            for (const sign of [-1, 1]) {
                box(0, sign * 1.03, -.12, 3.36, .18, .48);
                box(sign * 1.59, 0, -.12, .18, 1.88, .48);
                for (const x of [-1.30, 0, 1.30]) box(x, sign * 1.13, -.17, .15, .15, .33);
            }
        }),
        geometry(1, box => {
            box(0, 0, 0, 3.38, 2.26, .14);
            box(0, 0, .08, 2.55, 1.46, .09);
            for (const y of [-.47, 0, .47]) box(0, y, .15, 2.25, .11, .10);
        }),
        geometry(2, (_, ring) => ring(64, i => i % 4 === 1 || i % 4 === 2 ? .78 : .68, .20, .23)),
        geometry(3, (_, ring) => ring(48, i => i % 4 === 1 || i % 4 === 2 ? .61 : .52, .15, .23, Math.PI / 24)),
        geometry(4, (_, ring) => ring(6, () => .14, .047, .13)),
    ];
    const nodes = [
        part('housing', 0), part('cover', 1, [0, 0, .23]),
        part('drive_gear', 2, [-.63, 0, -.03]), part('driven_gear', 3, [.67, alternate ? .08 : 0, -.03]),
        part('bolt_fl', 4, [-1.43, -.88, .37]), part('bolt_fr', 4, [1.43, -.88, .37]),
        part('bolt_bl', 4, [-1.43, .88, .37]), part('bolt_br', 4, [1.43, .88, .37]),
    ];
    if (alternate) {
        meshes.reverse();
        nodes.reverse();
        for (const node of nodes) node.mesh = meshes.length - 1 - node.mesh;
    }
    // Nest the mesh nodes to exercise the importer’s assembled world transforms.
    const children = nodes.map((_, index) => index);
    nodes.push({ name: 'Gearbox assembly', translation: [0, .2, 0], children });
    return { colors, meshes, nodes };
}

function writeGlb(file, alternate) {
    const { colors, meshes, nodes } = gearbox(alternate);
    const bin = [];
    const gltf = {
        asset: { version: '2.0', generator: 'Minvex maintenance asset generator' },
        scene: 0, scenes: [{ nodes: [nodes.length - 1] }], nodes, meshes: [],
        materials: colors.map(color => ({ pbrMetallicRoughness: { baseColorFactor: [...color, 1], metallicFactor: .15, roughnessFactor: .55 } })),
        buffers: [], bufferViews: [], accessors: [],
    };
    function accessor(values, type, min, max) {
        const scalar = type === 'SCALAR';
        const offset = bin.reduce((size, chunk) => size + chunk.length, 0);
        const data = Buffer.alloc(values.length * (scalar ? 2 : 4));
        values.forEach((value, i) => scalar ? data.writeUInt16LE(value, i * 2) : data.writeFloatLE(value, i * 4));
        bin.push(data, Buffer.alloc((4 - data.length % 4) % 4));
        const bufferView = gltf.bufferViews.push({ buffer: 0, byteOffset: offset, byteLength: data.length }) - 1;
        return gltf.accessors.push({ bufferView, type, componentType: scalar ? 5123 : 5126,
            count: values.length / (scalar ? 1 : 3), min, max }) - 1;
    }
    for (const { material, positions, normals, indices } of meshes) {
        const bound = operation => [0, 1, 2].map(axis => operation(...positions.filter((_, i) => i % 3 === axis)));
        const POSITION = accessor(positions, 'VEC3', bound(Math.min), bound(Math.max));
        const NORMAL = accessor(normals, 'VEC3');
        gltf.meshes.push({ primitives: [{ attributes: { POSITION, NORMAL }, indices: accessor(indices, 'SCALAR'), material }] });
    }
    const binary = Buffer.concat(bin);
    gltf.buffers.push({ byteLength: binary.length });
    const json = JSON.stringify(gltf);
    const metadata = Buffer.from(json.padEnd(Math.ceil(json.length / 4) * 4));
    const words = (...values) => {
        const bytes = Buffer.alloc(values.length * 4);
        values.forEach((value, i) => bytes.writeUInt32LE(value, i * 4));
        return bytes;
    };
    writeFileSync(new URL(`../examples/assets/${file}`, import.meta.url), Buffer.concat([
        words(0x46546c67, 2, 28 + metadata.length + binary.length, metadata.length, 0x4e4f534a),
        metadata, words(binary.length, 0x004e4942), binary,
    ]));
}

writeGlb('MaintenanceGearbox.glb', false);
writeGlb('MaintenanceGearboxAlt.glb', true);
