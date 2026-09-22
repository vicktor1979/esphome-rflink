#!/usr/bin/env python3
"""Generate a small per-device ESPHome package for chosen RFLink fields.

Requires Python 3.9+ and PyYAML (python -m pip install pyyaml).
Uses the common rflink_fields.h from the v0.1.3 bridge extension.
Never creates, deletes or modifies any RFLink plugin or network setting.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import re
import sys
try:
    import yaml
except ImportError:
    raise SystemExit('Missing PyYAML: python -m pip install pyyaml')

ROOT = Path(__file__).resolve().parents[1]
FIELDS = {x['key']: x for x in json.loads((ROOT/'FIELD_MAP.json').read_text(encoding='utf-8'))}
BINARY = {
    'BAT': ('alacsony elem', 'battery', 'LOW', 'OK'),
    'PIR': ('mozgás', 'motion', 'ON', 'OFF'),
    'SMOKEALERT': ('füstjelzés', 'smoke', 'ON', 'OFF'),
}
TEXT = {'PARAM':'csomagsorszám','NAME':'protokoll','ID':'azonosító','SWITCH':'gomb',
        'CMD':'parancs','RGBW':'RGBW kód'}

class Literal(str): pass
class Lambda(str): pass
class Dumper(yaml.SafeDumper): pass
Dumper.add_representer(Literal, lambda d,v:d.represent_scalar('tag:yaml.org,2002:str',str(v),style='|'))
Dumper.add_representer(Lambda, lambda d,v:d.represent_scalar('!lambda',str(v)))
Dumper.add_representer(type(None),lambda d,v:d.represent_scalar('tag:yaml.org,2002:null',''))

def generate(prefix: str, name: str, protocol: str, rf_id: str, fields: list[str], stale_after: str) -> dict:
    """Return a package with exact NAME+ID filtering and per-field expiry."""
    if not re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*', prefix):
        raise ValueError('prefix must be a valid C++/ESPHome ID, e.g. kert_rf')
    for label,value in [('name',name),('protocol',protocol),('rf_id',rf_id)]:
        if not value or len(value)>120 or any(ord(c)<32 for c in value):
            raise ValueError(f'{label} must be a nonempty, short single-line string')
    if not re.fullmatch(r'[1-9][0-9]*(?:ms|s|min|h|d)', stale_after):
        raise ValueError('stale_after example: 60min, 120s, 2h')
    unknown = set(fields) - (set(FIELDS)|set(BINARY)|set(TEXT))
    if unknown: raise ValueError('Unknown fields: '+', '.join(sorted(unknown)))
    if not fields: raise ValueError('Select at least one field')
    cpp=lambda value:json.dumps(value,ensure_ascii=True)
    output={'json':None}
    lines=['const bool valid = json::parse_json(message, [&](JsonObject root) -> bool {',
           '  namespace data = esphome::rflink_data;',
           '  const std::string protocol = root["NAME"] | "";',
           '  const std::string device = root["ID"] | "";',
           f'  if (protocol != {cpp(protocol)} || device != {cpp(rf_id)}) return true;']
    for key in dict.fromkeys(fields):
        ident=prefix+'_'+key.lower()
        if key in FIELDS:
            f=FIELDS[key]
            ent={'platform':'template','id':ident,'name':name+' '+f['label'],
                 'icon':f['icon'],'internal':False,'accuracy_decimals':f['decimals'],
                 'update_interval':'never','filters':[{'timeout':stale_after}]}
            if f['unit']: ent['unit_of_measurement']=f['unit']
            if f['device_class']:ent['device_class']=f['device_class']
            if key in ['TEMP','HUM','WINSP','AWINSP','WINDIR','WINCHL','WINTMP','WATT']:
                ent['state_class']='measurement'
            if 'nyers' in f['label']:ent['entity_category']='diagnostic'
            output.setdefault('sensor',[]).append(ent)
            lines.append(f'  data::publish_number(id({ident}), data::number(root, data::{key}), false);')
        elif key in BINARY:
            label,dc,on,off=BINARY[key]
            ent={'platform':'template','id':ident,'name':name+' '+label,'device_class':dc,
                 'internal':False,'filters':[{'timeout':stale_after}]}
            if key=='BAT':ent['entity_category']='diagnostic'
            output.setdefault('binary_sensor',[]).append(ent)
            lines += [f'  const std::string value_{key} = root["{key}"] | "";',
                f'  data::publish_binary(id({ident}), data::binary_value(value_{key}, "{on}", "{off}"), !root["{key}"].isNull(), false);']
        else:
            output.setdefault('text_sensor',[]).append({'platform':'template','id':ident,
                'name':name+' '+TEXT[key],'internal':False,'entity_category':'diagnostic','update_interval':'never'})
            lines += [f'  if (root["{key}"].is<const char *>())',
                      f'    data::publish_text(id({ident}), root["{key}"] | "");']
    lines+=['  return true;','});','if (!valid) ESP_LOGW("rflink.device", "Hibas JSON a rogzitett RF-eszkozhoz.");']
    output['script']=[{'id':prefix+'_process','mode':'queued','max_runs':5,
                      'parameters':{'message':'string'},'then':[{'lambda':Literal('\n'.join(lines))}]}]
    # A list here and in the main YAML ensures ESPHome concatenates callbacks.
    output['rflink']={'on_message':[{'then':[{'script.execute':{
        'id':prefix+'_process','message':Lambda('return x;')}}]}]}
    return output

def main() -> None:
    p=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--prefix',required=True)
    p.add_argument('--name',required=True)
    p.add_argument('--protocol',required=True,help='Exact decoded NAME value')
    p.add_argument('--rf-id',required=True,help='Exact decoded ID, including leading zeros')
    p.add_argument('--fields',default='TEMP,HUM,BAT',help='Comma-separated field names, or ALL')
    p.add_argument('--stale-after',default='60min')
    p.add_argument('--output',type=Path,required=True)
    args=p.parse_args()
    keys=list(FIELDS)+list(BINARY)+list(TEXT) if args.fields.upper()=='ALL' else [s.strip().upper() for s in args.fields.split(',') if s.strip()]
    try:
        content=generate(args.prefix,args.name,args.protocol,args.rf_id,keys,args.stale_after)
        args.output.parent.mkdir(parents=True,exist_ok=True)
        if args.output.exists():raise ValueError('Output already exists; choose a new name or back it up first')
        header=('# Per-device RFLink field package, generated for an EXPLICIT NAME + ID.\n'
                '# Numeric/binary state becomes unknown on expiry; no fake OFF/OK readings.\n'
                '# Values from other devices cannot overwrite these entities.\n'
                '# RF events while the API-dependent decoder is paused are not buffered.\n'
                '# Text fields remain last-known; numeric/binary expiry is independent.\n\n')
        args.output.write_text(header+yaml.dump(content,Dumper=Dumper,allow_unicode=True,sort_keys=False,width=100),encoding='utf-8')
    except (ValueError,OSError) as e:p.error(str(e))
    print(f'Created {args.output}; include this as an additional package after v0.1.3.')

if __name__=='__main__': main()
