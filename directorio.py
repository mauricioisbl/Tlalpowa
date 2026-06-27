from pathlib import Path
from datetime import datetime

IGNORAR_DIRS = {'.git', '.vs', '__pycache__', 'build', '_build', 'out', 'CMakeFiles', '.cache', '.pytest_cache'}
IGNORAR_ARCHIVOS = {'rutas_directorio.txt', 'PUBLICAR.LOG'}


def visible(p: Path) -> bool:
    parts = set(p.parts)
    if parts & IGNORAR_DIRS:
        return False
    if p.name in IGNORAR_ARCHIVOS:
        return False
    if p.suffix.lower() in {'.obj', '.pdb', '.ilk', '.tlog', '.lastbuildstate', '.tmp', '.temp', '.log'}:
        return False
    return True


def main() -> int:
    base = Path(__file__).resolve().parent
    salida = base / 'rutas_directorio.txt'
    rutas = [p for p in base.rglob('*') if visible(p)]
    rutas.sort(key=lambda p: str(p).lower())
    with salida.open('w', encoding='utf-8', newline='\n') as f:
        f.write(f'Directorio base: {base}\n')
        f.write(f'Generado: {datetime.now().strftime("%Y-%m-%d %H:%M:%S")}\n\n')
        for ruta in rutas:
            f.write(str(ruta) + '\n')
    print(salida)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
