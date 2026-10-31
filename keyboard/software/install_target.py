#!/usr/bin/env python3
"""Prepare deployment files anywhere; apply only on a dedicated keyboard CM4."""
import argparse,hashlib,json,os,pwd,re,shutil,subprocess,tarfile,tempfile
from pathlib import Path
from urllib.request import urlopen

HERE=Path(__file__).resolve().parent
BASE='/opt/keyboard/rev-g'

def preview_root(path):
    path=path.absolute()
    if path != path.resolve() or path==Path('/'):
        raise ValueError('Preparation needs a real isolated directory, without symlink parents')
    if path.exists() and (not path.is_dir() or any(path.iterdir())):
        raise ValueError('Preparation destination must be empty or new')
    return path

def boot_text(text):
    text='\n'.join(line for line in text.splitlines() if line.strip()!='include keyboard-revf.txt')+'\n'
    active='\n'.join(line.strip() for line in text.splitlines() if not line.lstrip().startswith('#'))
    if re.search(r'^dtoverlay=(?:pi3-)?disable-bt(?:,|$)',active,re.M):
        raise ValueError('Remove the explicit disable-bt overlay from this dedicated CM4 config to enable headphones')
    if 'dtoverlay=vc4-fkms' in active:
        raise ValueError('Use the stock KMS desktop configuration, not FKMS')
    if 'dtoverlay=vc4-kms-v3d' not in active:text+='\n[all]\ndtoverlay=vc4-kms-v3d\n'
    if 'include keyboard-revg.txt' not in active:text+='\n[all]\ninclude keyboard-revg.txt\n'
    return text

def unit(description,command,user='root',extra='',after='local-fs.target'):
    return f'''[Unit]
Description={description}
After={after}
ConditionPathExists=/etc/keyboard/rev-g-target

[Service]
Type=simple
User={user}
WorkingDirectory={BASE}
ExecStart={command}
Restart=on-failure
RestartSec=2
{extra}
[Install]
WantedBy=multi-user.target
'''

def prepare(root,user,uid=1000):
    def put(name,text,mode=0o644):
        p=root/name.lstrip('/');p.parent.mkdir(parents=True,exist_ok=True);p.write_text(text);p.chmod(mode)
    destination=root/BASE.lstrip('/');destination.mkdir(parents=True,exist_ok=True)
    shutil.copytree(HERE/'keyboard_ui',destination/'keyboard_ui',ignore=shutil.ignore_patterns('__pycache__'),dirs_exist_ok=True)
    shutil.copytree(HERE/'target',destination/'target',dirs_exist_ok=True)
    images=HERE.parent/'firmware/binaries'
    shutil.copytree(images/'esp32',destination/'firmware/esp32',dirs_exist_ok=True)
    shutil.copy2(images/'manifest.json',destination/'firmware/manifest.json')
    put('/etc/keyboard/rev-g-target','Dedicated keyboard CM4\n')
    put('/etc/asound.conf',(HERE/'target/asound.conf').read_text())
    put('/boot/firmware/keyboard-revg.txt',(HERE/'target/boot-config.txt').read_text())
    py='/usr/bin/python3'
    audio_env=f'Environment=XDG_RUNTIME_DIR=/run/user/{uid}\nEnvironment=DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/{uid}/bus\n'
    services={
      'keyboard-ui.service':unit('Keyboard touchscreen and macro service',f'{py} -m keyboard_ui.app --hardware --data /var/lib/keyboard',user,'RuntimeDirectory=keyboard\nRuntimeDirectoryMode=0700\nStateDirectory=keyboard\nStateDirectoryMode=0700\nUMask=0077\nNoNewPrivileges=true\n', 'local-fs.target sound.target'),
      'keyboard-usb-audio.service':unit('Keyboard USB speaker connection',f'{py} {BASE}/target/usb_audio.py'),
      'keyboard-audio-loop.service':unit('PC audio to keyboard stereo mixer',f'{py} {BASE}/target/audio_loop.py',user,after='keyboard-usb-audio.service sound.target'),
      'keyboard-plexamp.service':unit('Independent Plexamp music player',f'/opt/keyboard/node/bin/node /opt/keyboard/plexamp/js/index.js',user,'Environment=HOME=/home/'+user+'\n', 'network-online.target sound.target')}
    services['keyboard-plexamp.service']=services['keyboard-plexamp.service'].replace(f'WorkingDirectory={BASE}','WorkingDirectory=/opt/keyboard/plexamp')
    for name,text in services.items():
        if name in ['keyboard-ui.service','keyboard-audio-loop.service','keyboard-plexamp.service']:
            text=text.replace('[Service]',f'Requires=user@{uid}.service\nAfter=user@{uid}.service\n\n[Service]\n'+audio_env)
        put('/etc/systemd/system/'+name,text)
    put('/etc/wireplumber/wireplumber.conf.d/60-keyboard-audio.conf','''# Dedicated keyboard user also owns Bluetooth audio without a seat login.
wireplumber.profiles = { main = { monitor.bluez.seat-monitoring = disabled } }
wireplumber.settings = { linking.follow-default-target = true }
''')
    put('/etc/systemd/system/keyboard-poweroff.path','''[Unit]
Description=Keyboard hardware shutdown request
After=keyboard-ui.service
ConditionPathExists=/etc/keyboard/rev-g-target
[Path]
PathExists=/run/keyboard/poweroff-request
Unit=keyboard-poweroff.service
[Install]
WantedBy=multi-user.target
''')
    put('/etc/systemd/system/keyboard-poweroff.service',f'''[Unit]
Description=Finish keyboard shutdown before power removal
ConditionPathExists=/etc/keyboard/rev-g-target
[Service]
Type=oneshot
ExecStart={py} {BASE}/target/poweroff.py
''')
    put('/etc/udev/rules.d/90-keyboard-backlight.rules',f'ACTION=="add", SUBSYSTEM=="backlight", RUN+="/bin/chgrp video /sys%p/brightness", RUN+="/bin/chmod g+w /sys%p/brightness"\n')
    put(f'/home/{user}/.config/autostart/keyboard-screen.desktop',f'''[Desktop Entry]
Type=Application
Name=Keyboard screen
Exec=chromium --kiosk --no-first-run --disable-session-crashed-bubble --ozone-platform=wayland --enable-wayland-ime --wayland-text-input-version=3 http://127.0.0.1:8765
Terminal=false
''')
    # Enable the dedicated user's audio session at boot. Keep all preview writes inside root.
    for name in ['pipewire.socket','pipewire-pulse.socket','wireplumber.service']:
        p=root/f'home/{user}/.config/systemd/user/default.target.wants/{name}';p.parent.mkdir(parents=True,exist_ok=True)
        if p.is_symlink():p.unlink()
        elif p.exists():raise ValueError('Unexpected existing audio unit file: '+str(p))
        p.symlink_to('/usr/lib/systemd/user/'+name)
    put('/opt/keyboard/release-manifest.json',(HERE/'release-manifest.json').read_text())
    return list(services)+['keyboard-poweroff.path']

def fetch_extract(entry,destination):
    with tempfile.TemporaryDirectory(prefix='keyboard-download-') as tmp:
        archive=Path(tmp)/'release.tar';digest=hashlib.sha256()
        with urlopen(entry['url'],timeout=60) as response,archive.open('wb') as out:
            while chunk:=response.read(1024*1024):digest.update(chunk);out.write(chunk)
        if digest.hexdigest()!=entry['sha256']:raise RuntimeError('Release checksum mismatch; nothing extracted')
        unpack=Path(tmp)/'unpacked';unpack.mkdir()
        with tarfile.open(archive) as tar:tar.extractall(unpack,filter='data')
        children=list(unpack.iterdir())
        if len(children)!=1 or not children[0].is_dir():raise RuntimeError('Unexpected release archive layout')
        destination.mkdir(parents=True,exist_ok=True);shutil.copytree(children[0],destination,dirs_exist_ok=True)

def main():
    parser=argparse.ArgumentParser(description=__doc__);group=parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--prepare-directory',type=Path);group.add_argument('--apply-on-cm4',action='store_true')
    parser.add_argument('--desktop-user',required=True);parser.add_argument('--desktop-uid',type=int,default=1000);args=parser.parse_args();user=args.desktop_user
    if not re.fullmatch('[a-z_][a-z0-9_-]{0,30}',user):parser.error('Invalid target user name')
    if args.desktop_uid<1:parser.error('Desktop UID must be a positive integer')
    if args.prepare_directory:
        try:root=preview_root(args.prepare_directory)
        except ValueError as e:parser.error(str(e))
        prepare(root,user,args.desktop_uid);print('Prepared target filesystem in',root);return
    # No apt, downloads, writes or service changes precede this guard.
    model=Path('/proc/device-tree/model')
    if not model.exists() or b'Compute Module 4' not in model.read_bytes():parser.error('Refusing installation: this computer is not the keyboard CM4')
    if os.geteuid()!=0:parser.error('Run the target installation as root on the keyboard CM4')
    if os.uname().machine!='aarch64':parser.error('Use 64-bit Raspberry Pi OS Desktop')
    account=pwd.getpwnam(user)
    if account.pw_dir!='/home/'+user:parser.error('Expected a regular desktop user with /home/<name>')
    boot=Path('/boot/firmware');config=boot/'config.txt';cmdline=boot/'cmdline.txt'
    if not config.exists() or not cmdline.exists():parser.error('Expected Raspberry Pi OS boot files in /boot/firmware')
    try:configured_boot=boot_text(config.read_text())
    except ValueError as e:parser.error(str(e))
    if not hasattr(tarfile,'data_filter'):parser.error('Python with safe tar extraction support is required')
    # Check target paths before installing anything. Never follow a user-config symlink.
    managed=[Path('/etc/asound.conf'),Path('/etc/keyboard'),Path('/opt/keyboard'),
             Path('/home')/user/'.config',Path('/home')/user/'.config/autostart',
             Path('/home')/user/'.config/systemd',Path('/home')/user/'.config/systemd/user']
    for path in managed:
        if path.is_symlink() or path.absolute()!=path.resolve():parser.error('Target managed path is a symlink: '+str(path))
    backup=Path('/var/backups/keyboard-revg');backup.mkdir(parents=True,exist_ok=True)
    for p in [config,cmdline,Path('/etc/asound.conf')]:
        if p.exists() and not (backup/p.name).exists():shutil.copy2(p,backup/p.name)
    user_config=Path('/home')/user/'.config'
    if user_config.exists() and not (backup/'desktop-config').exists():
        shutil.copytree(user_config,backup/'desktop-config',symlinks=True)
    subprocess.run(['apt-get','update'],check=True)
    subprocess.run(['apt-get','install','-y','python3','python3-dbus','alsa-utils','libasound2','chromium','bluez','pipewire','pipewire-pulse','pipewire-alsa','wireplumber','libspa-0.2-bluetooth','esptool','lrzsz'],check=True)
    subprocess.run(['usermod','-aG','audio,video,render,input,dialout',user],check=True)
    releases=json.loads((HERE/'release-manifest.json').read_text())
    fetch_extract(releases['node'],Path('/opt/keyboard/node'));fetch_extract(releases['plexamp'],Path('/opt/keyboard/plexamp'))
    version=subprocess.run(['wireplumber','--version'],capture_output=True,text=True,check=True).stdout
    if not re.search(r'0\.[5-9]\.|[1-9]\.\d+\.',version):raise RuntimeError('Use Raspberry Pi OS 64-bit Trixie with WirePlumber 0.5 or later')
    # These masks were written by Rev F only. Do not silently remove another installation's masks.
    if Path('/etc/keyboard/rev-f-target').is_file():
        for name in ['pipewire.service','pipewire.socket','pipewire-pulse.service','pipewire-pulse.socket','wireplumber.service']:
            mask=Path('/home')/user/'.config/systemd/user'/name
            if mask.is_symlink() and os.readlink(mask)=='/dev/null':mask.unlink()
    units=prepare(Path('/'),user,account.pw_uid)
    if config.read_text()!=configured_boot:config.write_text(configured_boot)
    tokens=[x for x in cmdline.read_text().split() if not re.match(r'console=(serial0|ttyAMA0|ttyS0),',x)]
    cmdline.write_text(' '.join(tokens)+'\n')
    # Change ownership only of project-created entries, never recursively across the desktop.
    for rel in ['', 'autostart','systemd','systemd/user','autostart/keyboard-screen.desktop']:
        os.chown(user_config/rel,account.pw_uid,account.pw_gid,follow_symlinks=False)
    wants=user_config/'systemd/user/default.target.wants'
    os.chown(wants,account.pw_uid,account.pw_gid,follow_symlinks=False)
    for p in wants.iterdir():
        if p.name in ['pipewire.socket','pipewire-pulse.socket','wireplumber.service']:os.chown(p,account.pw_uid,account.pw_gid,follow_symlinks=False)
    subprocess.run(['loginctl','enable-linger',user],check=True)
    subprocess.run(['systemctl','enable','hciuart.service','bluetooth.service','serial-getty@ttyGS0.service'],check=True)
    subprocess.run(['systemctl','daemon-reload'],check=True)
    # Plexamp must be claimed interactively first. Starting waits until the next target reboot.
    subprocess.run(['systemctl','enable',*[x for x in units if x!='keyboard-plexamp.service']],check=True)
    print('Target installation prepared. Enable desktop autologin, claim Plexamp as the desktop user, then reboot the CM4. See software/README.md.')
if __name__=='__main__':main()
