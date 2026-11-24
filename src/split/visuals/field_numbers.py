"""Serialize only SVG numeric fields into the OP-1 Field's decimal grammar."""
from decimal import Decimal, ROUND_HALF_EVEN
import re

NUMBER = re.compile(r'[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?')
# The published retained scenes emit these fields. Unknown fields require an
# explicit choice so a newly added opacity or coordinate cannot skip this pass.
NUMERIC = {'d','transform','viewBox','points','stroke-dasharray',
           'width','height','x','y','cx','cy','x1','y1','x2','y2','r','rx','ry',
           'stroke-width','stroke-opacity','fill-opacity','opacity','stroke-miterlimit',
           'stroke-dashoffset','font-size'}
LITERAL = {'id','xmlns','display','fill','stroke','stroke-linejoin','stroke-linecap','fill-rule'}
FIELD_NUMBER = re.compile(r'-?(?:0|[1-9][0-9]*)(?:\.[0-9]{1,3})?\Z')
QUANTUM = Decimal('0.001')


def field_number(token):
    value = Decimal(str(token)).quantize(QUANTUM,rounding=ROUND_HALF_EVEN)
    if not value:
        return '0'
    return format(value,'f').rstrip('0').rstrip('.')


PATH_LEXEME = re.compile(NUMBER.pattern + r'|[MmZzLlHhVvCcSs]')
PATH_ARGUMENTS = dict(M=2,L=2,H=1,V=1,C=6,S=4,Z=0)
TRANSFORM = re.compile(r'(translate|scale|rotate|matrix|skewX|skewY)\s*\(([^()]*)\)')
TRANSFORM_COUNTS = {'translate':(1,2), 'scale':(1,2), 'rotate':(1,3), 'matrix':(6,),
                    'skewX':(1,), 'skewY':(1,)}


def scan(pattern, value):
    previous=0
    for match in pattern.finditer(value):
        if value[previous:match.start()].strip(' ,\t\r\n'):
            raise ValueError(f'Unexpected content in SVG field {value!r}')
        yield match
        previous=match.end()
    if value[previous:].strip(' ,\t\r\n'):
        raise ValueError(f'Unexpected content in SVG field {value!r}')


def path_field(value):
    # Use one command per segment. An implicit pair after M/m is a line.
    output=[]
    command=None
    pending=[]
    awaiting=False
    for match in scan(PATH_LEXEME,value):
        token=match.group()
        if token[0].isalpha():
            if pending or awaiting:
                raise ValueError(f'Incomplete SVG path {value!r}')
            command=token
            if not output and command.upper()!='M':
                raise ValueError(f'SVG path must start with move: {value!r}')
            awaiting=command.upper()!='Z'
            if command.upper()=='Z':
                output.append(command)
                command=None
        else:
            if command is None:
                raise ValueError(f'Missing SVG path command in {value!r}')
            pending.append(field_number(token))
            if len(pending)==PATH_ARGUMENTS[command.upper()]:
                output.extend([command,*pending])
                pending=[]
                awaiting=False
                if command=='M':command='L'
                if command=='m':command='l'
    if pending or awaiting or not output:
        raise ValueError(f'Incomplete SVG path {value!r}')
    return ' '.join(output)


def numbers(value):
    raw=[match.group() for match in scan(NUMBER,value)]
    if not raw:raise ValueError(f'No numeric values in {value!r}')
    return [field_number(v) for v in raw]


def number_field(key,value):
    if key=='d':
        return path_field(value)
    if key=='transform':
        parts=[]
        for match in scan(TRANSFORM,value):
            name=match[1]
            args=numbers(match[2])
            if len(args) not in TRANSFORM_COUNTS[name]:
                raise ValueError(f'Wrong transform arity in {value!r}')
            parts.append(name+'('+' '.join(args)+')')
        if not parts:raise ValueError(f'No transforms in {value!r}')
        return ' '.join(parts)
    if key=='stroke-dasharray' and value=='none':return value
    args=numbers(value)
    if key=='viewBox' and len(args)!=4:
        raise ValueError(f'Wrong viewBox arity {value!r}')
    if key not in {'viewBox','stroke-dasharray','points'} and len(args)!=1:
        raise ValueError(f'Wrong arity for {key}={value!r}')
    return ' '.join(args)


def normalize_svg(root):
    for node in root.iter():
        for key,value in tuple(node.attrib.items()):
            if key in NUMERIC:
                node.set(key,number_field(key,value))
            elif key not in LITERAL:
                raise ValueError(f'Unclassified SVG attribute {key} on {node.tag}')


def verify_svg(root):
    """Audit the final decoded picture, including fields that were hand-edited."""
    count=0
    for node in root.iter():
        for key,value in node.attrib.items():
            if key in LITERAL:
                continue
            if key not in NUMERIC:
                raise ValueError(f'Unclassified SVG attribute {key} on {node.tag}')
            for match in NUMBER.finditer(value):
                token=match.group()
                if not FIELD_NUMBER.fullmatch(token):
                    raise ValueError(f'{node.tag} {key} uses unsupported numeric literal {token}')
                count+=1
            # Also reject corrupted number lists, unknown CSS units, NaN/Inf,
            # or adjacent numbers which canonicalization would accidentally join.
            if number_field(key,value)!=value:
                raise ValueError(f'{node.tag} {key} is not canonical: {value!r}')
    return count
