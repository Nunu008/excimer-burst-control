# PyInstaller spec: single-file executable of the dashboard for Windows and Linux.
#
#   pip install -r requirements.txt -r packaging/requirements-packaging.txt
#   pyinstaller packaging/excimer_burst_control.spec --noconfirm --clean
#
# Output: dist/ExcimerBurstControl(.exe). Verify it with
#   dist/ExcimerBurstControl --self-test --report selftest.txt
# ruff: noqa
from pathlib import Path

ROOT = Path(SPECPATH).parent  # noqa: F821 - provided by PyInstaller

a = Analysis(
    [str(ROOT / "main.py")],
    pathex=[str(ROOT)],
    # pyqtgraph picks its Qt templates at run time by binding name.
    hiddenimports=[
        "pyqtgraph.graphicsItems.ViewBox.axisCtrlTemplate_pyside6",
        "pyqtgraph.graphicsItems.PlotItem.plotConfigTemplate_pyside6",
        "pyqtgraph.imageview.ImageViewTemplate_pyside6",
    ],
    excludes=["tkinter", "matplotlib", "scipy", "IPython", "PyQt5", "PyQt6"],
    noarchive=False,
)
pyz = PYZ(a.pure)  # noqa: F821

exe = EXE(  # noqa: F821
    pyz,
    a.scripts,
    a.binaries,
    a.datas,
    [],
    name="ExcimerBurstControl",
    console=False,  # a desktop application: no console window on Windows
    upx=False,
    strip=False,
    runtime_tmpdir=None,
)
