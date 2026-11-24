"""Create seventeen retained waist shapes from the original etched bottle paths."""
from copy import deepcopy
from math import sqrt
from xml.etree import ElementTree as ET

import numpy as np
from svgpathtools import CubicBezier, Line, QuadraticBezier, parse_path

AXIS = complex(2, 1) / sqrt(5)
NORMAL = complex(-AXIS.imag, AXIS.real)
CENTER = complex(184, 93)
HALF_LENGTH = 26.0
POSES = 17
MAX_COMPRESSION = 0.54
NS = '{http://www.w3.org/2000/svg}'


def dot(a, b):
    return (a * b.conjugate()).real


def longitudinal(point):
    return dot(point-CENTER, AXIS)


def deform(point, strength):
    s = longitudinal(point)/HALF_LENGTH
    if abs(s) >= 1:
        return point
    weight = (1-s*s)**2
    return point - strength*MAX_COMPRESSION*weight*dot(point-CENTER,NORMAL)*NORMAL


def tangent(point, direction, strength):
    s = longitudinal(point)/HALF_LENGTH
    if abs(s) >= 1:
        return direction
    w = (1-s*s)**2
    dw = -4*s*(1-s*s)/HALF_LENGTH
    offset = dw*dot(direction,AXIS)*dot(point-CENTER,NORMAL) + w*dot(direction,NORMAL)
    return direction - strength*MAX_COMPRESSION*offset*NORMAL


def warped_segment(segment, strength, lo, hi):
    mid = (lo+hi)*.5
    if abs(longitudinal(segment.point(mid))) >= HALF_LENGTH:
        # Edges in the base and neck regions remain the exact original curves.
        return [segment.cropped(lo,hi)]
    def cubic(a,b):
        pa,pb = segment.point(a),segment.point(b)
        qa,qb = deform(pa,strength),deform(pb,strength)
        h = (b-a)/3
        candidate = CubicBezier(qa,qa+h*tangent(pa,segment.derivative(a),strength),
                                qb-h*tangent(pb,segment.derivative(b),strength),qb)
        err = max(abs(candidate.point(t)-deform(segment.point(a+t*(b-a)),strength))
                  for t in (.25,.5,.75))
        if err < .022:
            return [candidate]
        half=(a+b)*.5
        return cubic(a,half)+cubic(half,b)
    return cubic(lo,hi)


def split_points(segment):
    # The only boundaries of this deformation are two lines normal to the axis.
    # Solve for their intersections, preserving whole untouched portions at both ends.
    coeff = [dot(z,AXIS) for z in segment.poly().coeffs]
    coeff[-1] -= dot(CENTER,AXIS)
    points = [0.,1.]
    for boundary in (-HALF_LENGTH,HALF_LENGTH):
        polynomial = coeff.copy()
        polynomial[-1] -= boundary
        points.extend(float(t.real) for t in np.roots(polynomial)
                      if abs(t.imag)<1e-7 and 1e-7<t.real<1-1e-7)
    return sorted(set(points))


def fmt(number):
    return f'{number:.3f}'.rstrip('0').rstrip('.') or '0'


def xy(point):
    return fmt(point.real)+' '+fmt(point.imag)


def serialize(segment):
    if isinstance(segment,Line):
        return 'L'+xy(segment.end)
    if isinstance(segment,QuadraticBezier):
        segment = CubicBezier(segment.start,(segment.start+2*segment.control)/3,
                              (segment.end+2*segment.control)/3,segment.end)
    return 'C'+' '.join(xy(p) for p in (segment.control1,segment.control2,segment.end))


def prepare(root):
    body = root.find('.//*[@id="syrupBody"]')
    layers = [ET.Element(NS+'g',{'id':f'syrupWaist{i:02d}'}) for i in range(POSES)]
    original = list(body)
    # Only contours that meet the central band need more than one pose. Static base
    # and neck etchings are emitted once. Keep the filled silhouette behind the etched lines.
    for node in original:
        if node.tag != NS+'path':
            continue
        path = parse_path(node.get('d'))
        parts = []
        for subpath in path.continuous_subpaths():
            pieces = [(seg,split_points(seg)) for seg in subpath]
            parts.append((subpath,pieces))
        moving = any(abs(longitudinal(seg.point((a+b)/2))) < HALF_LENGTH
                     for _,pieces in parts for seg,cuts in pieces for a,b in zip(cuts,cuts[1:]))
        if not moving:
            continue
        body.remove(node)
        for i,layer in enumerate(layers):
            item = deepcopy(node)
            # The neutral pose uses the same boundary subdivisions and numeric
            # rounding as all active poses; the renderer sees identical fixed ends.
            strength = i/(POSES-1)
            cmds = []
            for sub,pieces in parts:
                cmds.append('M'+xy(deform(sub.start,strength)))
                for seg,cuts in pieces:
                    for a,b in zip(cuts,cuts[1:]):
                        cmds.extend(serialize(s) for s in warped_segment(seg,strength,a,b))
                if sub.isclosed():
                    cmds.append('Z')
            item.set('d',' '.join(cmds))
            layer.append(item)
    for i,layer in enumerate(layers):
        if i: layer.set('display','none')
        body.insert(i,layer)
