"""Explicit remote commands: learn a remote, import Pronto, or try a known NEC code."""
import re

def validate(code):
    if not isinstance(code,dict):raise ValueError('IR command must be an object')
    hz=code.get('frequency');pulses=code.get('pulses')
    if not isinstance(hz,int) or not 25000<=hz<=60000:raise ValueError('IR carrier must be 25–60 kHz')
    if not isinstance(pulses,list) or not 2<=len(pulses)<=1024:raise ValueError('IR command must contain 2–1024 durations')
    if any(type(x) is not int or not 80<=x<=32767 for x in pulses):raise ValueError('Each IR duration must be 80–32767 microseconds')
    if sum(pulses)>500000:raise ValueError('IR frame exceeds 500 ms')
    return {'frequency':hz,'pulses':pulses}

def pronto(value):
    try: words=[int(x,16) for x in value.split()]
    except (ValueError,AttributeError):raise ValueError('Enter Pronto hexadecimal words')
    if len(words)<6 or words[0]!=0 or not words[1]:raise ValueError('Use raw modulated Pronto format 0000')
    if len(words)!=4+2*(words[2]+words[3]):raise ValueError('Pronto length does not match its header')
    period=words[1]*0.241246
    # Prefer the complete intro frame. Repeat-only codes use the repeat frame.
    data=words[4:4+2*words[2]] if words[2] else words[4:]
    return validate({'frequency':round(1000000/period),'pulses':[round(x*period) for x in data]})

def nec(address,command,extended=False):
    if type(address) is not int or not 0<=address<=(65535 if extended else 255):raise ValueError('Invalid NEC address')
    if type(command) is not int or not 0<=command<=255:raise ValueError('Invalid NEC command')
    data=[address&255,address>>8 if extended else address^255,command,command^255]
    pulses=[9000,4500]
    for byte in data:
        for bit in range(8):pulses.extend([560,1690 if byte&(1<<bit) else 560])
    pulses.append(560)
    return validate({'frequency':38000,'pulses':pulses})
