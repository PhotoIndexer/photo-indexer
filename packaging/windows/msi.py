"""Instalador ``.msi`` do Windows (x64 e arm64) — gerado no Linux pelo wixl (msitools).

O wixl compila um subconjunto do WiX 3 sem precisar de Windows. O
``wixl-heat`` transforma a pasta do app (PhotoIDX.exe, DLLs, plugins/,
qml/, models/) em componentes; o ``PhotoIDX.wxs`` montado aqui
põe por cima o produto, os atalhos e o upgrade:

* instala por máquina em ``C:\\Program Files\\PhotoIDX`` (pede elevação);
* atalhos no Menu Iniciar e na Área de Trabalho para o ``PhotoIDX.exe``;
* a foto privada ``.photoidxe`` abre no PhotoIDX (dois cliques), com o ícone
  do app;
* aparece em "Aplicativos instalados" com ícone, versão e link;
* ``MajorUpgrade``: instalar uma versão nova remove a anterior; instalar uma
  mais velha por cima é recusado.

ARM64: o wixl 0.103 só conhece x86/x64. O pacote arm64 é gerado como x64
(componentes de 64 bits, ``ProgramFiles64Folder``) e depois o modelo do
resumo (``Template``) vira ``Arm64;1033`` com o ``msibuild`` — é o campo que
o Windows Installer usa para a plataforma do pacote.

**Nunca troque os GUIDs abaixo.** O ``UpgradeCode`` é o que faz o Windows
reconhecer as versões como o mesmo produto (x64 e arm64 dividem o mesmo:
são o mesmo programa); os dos componentes dos atalhos identificam o que
desinstalar.

Uso:
    msi.py --dir <pasta do app> --version 1.2.3 --arch x64|arm64 --out dist/X.msi
"""
import argparse
import os
import subprocess
import sys
from pathlib import Path
from xml.sax.saxutils import quoteattr

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import art  # noqa: E402  (packaging/art.py)

APP_NAME = 'PhotoIDX'
MANUFACTURER = 'PhotoE'
ABOUT_URL = 'https://photoe.com.br/'
UPGRADE_CODE = '706E99CD-1472-472D-9CBB-6F4945894927'
START_MENU_COMPONENT = 'CC4AAB61-4487-4A41-854B-778F968E70B1'
DESKTOP_COMPONENT = '0571D1B9-3F93-4856-A163-074759139CC6'
FILE_TYPE_COMPONENT = '93C83EBD-C925-4B69-B305-F96BC55E803D'
FILE_PROGID = 'PhotoIDX.EncryptedPhoto'

WIXL = os.environ.get('WIXL', 'wixl')
WIXL_HEAT = os.environ.get('WIXL_HEAT', 'wixl-heat')
MSIBUILD = os.environ.get('MSIBUILD', 'msibuild')
MSIINFO = os.environ.get('MSIINFO', 'msiinfo')


def msi_version(version: str) -> str:
    """X.Y.Z do projeto -> ProductVersion do MSI.

    O MSI aceita no maximo 255.255.65535, e a versao do projeto (a tag da
    Release, regra 17) vai ate 255.999.999. Os dois numeros de baixo viram um
    contador so
    (Y*1000 + Z, ate 999999) repartido em 16 bits: a ordem se mantem, que e o
    que o MajorUpgrade compara.
    """
    major, minor, patch = (int(p) for p in version.split('.'))
    counter = minor * 1000 + patch
    return f'{major}.{counter // 65536}.{counter % 65536}'


# Sem Platform no <Package>: o wixl nao le esse atributo (reclama e ignora);
# a plataforma x64 vem do ``--arch x64`` na linha de comando.
def _main_wxs(version: str, icon: Path) -> str:
    target = f'[INSTALLDIR]{APP_NAME}.exe'

    def shortcut(component_id, guid, directory, name):
        return f"""
    <DirectoryRef Id="{directory}">
      <Component Id="{component_id}" Guid="{guid}">
        <Shortcut Id="{component_id}Link" Name="{APP_NAME}" Description="{APP_NAME}"
                  Target={quoteattr(target)}
                  WorkingDirectory="INSTALLDIR" Icon="AppIcon.ico" />
        <RegistryValue Root="HKLM" Key="Software\\{MANUFACTURER}\\{APP_NAME}" Name="{name}"
                       Type="integer" Value="1" KeyPath="yes" />
      </Component>
    </DirectoryRef>"""

    return f"""<?xml version="1.0" encoding="utf-8"?>
<Wix xmlns="http://schemas.microsoft.com/wix/2006/wi">
  <Product Id="*" Name="{APP_NAME}" Language="1033" Version="{msi_version(version)}"
           Manufacturer="{MANUFACTURER}" UpgradeCode="{UPGRADE_CODE}">
    <Package InstallerVersion="500" Compressed="yes" InstallScope="perMachine"
             Description="{APP_NAME} {version}"
             Comments="Photo catalog with AI categories, faces and places" Manufacturer="{MANUFACTURER}" />
    <Media Id="1" Cabinet="app.cab" EmbedCab="yes" />
    <MajorUpgrade DowngradeErrorMessage="A newer version of {APP_NAME} is already installed." />

    <Icon Id="AppIcon.ico" SourceFile={quoteattr(str(icon))} />
    <Property Id="ARPPRODUCTICON" Value="AppIcon.ico" />
    <Property Id="ARPURLINFOABOUT" Value="{ABOUT_URL}" />
    <Property Id="ARPNOMODIFY" Value="1" />

    <Directory Id="TARGETDIR" Name="SourceDir">
      <Directory Id="ProgramFiles64Folder">
        <Directory Id="INSTALLDIR" Name="{APP_NAME}" />
      </Directory>
      <Directory Id="ProgramMenuFolder" />
      <Directory Id="DesktopFolder" />
    </Directory>
{shortcut('StartMenuShortcut', START_MENU_COMPONENT, 'ProgramMenuFolder', 'StartMenuShortcut')}
{shortcut('DesktopShortcut', DESKTOP_COMPONENT, 'DesktopFolder', 'DesktopShortcut')}

    <!-- .photoidxe -> PhotoIDX (RegistryValue: o wixl não monta ProgId/Extension). -->
    <DirectoryRef Id="INSTALLDIR">
      <Component Id="FileTypeAssociation" Guid="{FILE_TYPE_COMPONENT}">
        <RegistryValue Root="HKLM" Key="Software\\Classes\\.photoidxe" Type="string" Value="{FILE_PROGID}" KeyPath="yes" />
        <RegistryValue Root="HKLM" Key="Software\\Classes\\.photoidxe" Name="Content Type" Type="string" Value="application/x-photoidxe" />
        <RegistryValue Root="HKLM" Key="Software\\Classes\\{FILE_PROGID}" Type="string" Value="PhotoIDX encrypted photo" />
        <RegistryValue Root="HKLM" Key="Software\\Classes\\{FILE_PROGID}\\DefaultIcon" Type="string" Value="[INSTALLDIR]{APP_NAME}.exe,0" />
        <RegistryValue Root="HKLM" Key="Software\\Classes\\{FILE_PROGID}\\shell\\open\\command" Type="string"
                       Value="&quot;[INSTALLDIR]{APP_NAME}.exe&quot; &quot;%1&quot;" />
      </Component>
    </DirectoryRef>

    <Feature Id="Main" Title="{APP_NAME}" Level="1">
      <ComponentGroupRef Id="AppFiles" />
      <ComponentRef Id="StartMenuShortcut" />
      <ComponentRef Id="DesktopShortcut" />
      <ComponentRef Id="FileTypeAssociation" />
    </Feature>
  </Product>
</Wix>
"""


def build_msi(package_dir: Path, out_msi: Path, version: str, work: Path, arch: str = 'x64') -> Path:
    """``out_msi`` a partir da pasta do app (o que o bundle.py completou)."""
    package_dir = package_dir.resolve()
    if not (package_dir / f'{APP_NAME}.exe').is_file():
        raise SystemExit(f'{package_dir}: sem {APP_NAME}.exe')
    work.mkdir(parents=True, exist_ok=True)
    icon = art.write_ico(work / 'PhotoIDX.ico')

    files = sorted(str(p) for p in package_dir.rglob('*') if p.is_file())
    heat = subprocess.run(
        [WIXL_HEAT, '--prefix', f'{package_dir}/', '--directory-ref', 'INSTALLDIR',
         '--component-group', 'AppFiles', '--var', 'var.SourceDir', '--win64'],
        input='\n'.join(files) + '\n', capture_output=True, text=True, check=True)
    (work / 'files.wxs').write_text(heat.stdout)
    (work / 'main.wxs').write_text(_main_wxs(version, icon))

    out_msi.parent.mkdir(parents=True, exist_ok=True)
    out_msi.unlink(missing_ok=True)
    subprocess.run([WIXL, '--arch', 'x64', '-D', f'SourceDir={package_dir}', '-D', 'Win64=yes',
                    '-o', str(out_msi), str(work / 'main.wxs'), str(work / 'files.wxs')],
                   check=True)
    if arch == 'arm64':
        _set_template(out_msi, 'Arm64;1033')
    return out_msi


def _suminfo(msi: Path) -> dict:
    out = subprocess.run([MSIINFO, 'suminfo', str(msi)], check=True, capture_output=True, text=True).stdout
    return dict(line.split(': ', 1) for line in out.splitlines() if ': ' in line)


def _set_template(msi: Path, template: str):
    """Troca a plataforma do resumo, mantendo assunto, autor e o código do pacote."""
    info = _suminfo(msi)
    subprocess.run([MSIBUILD, str(msi), '-s', info.get('Subject', APP_NAME), info.get('Author', MANUFACTURER),
                    template, info['Revision number (UUID)']], check=True)
    got = _suminfo(msi).get('Template')
    if got != template:
        raise SystemExit(f'{msi}: Template ficou {got!r}, esperado {template!r}')


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--dir', required=True, type=Path)
    ap.add_argument('--version', required=True, help='X.Y.Z (a tag da Release)')
    ap.add_argument('--arch', choices=['x64', 'arm64'], default='x64')
    ap.add_argument('--out', required=True, type=Path)
    ap.add_argument('--work', type=Path)
    args = ap.parse_args()
    work = args.work or args.out.with_name(args.out.name + '.work')
    print(build_msi(args.dir, args.out, args.version, work, args.arch))


if __name__ == '__main__':
    main()
