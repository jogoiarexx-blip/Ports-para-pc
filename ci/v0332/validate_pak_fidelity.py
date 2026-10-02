from pathlib import Path
import sys
root=Path(sys.argv[1]) if len(sys.argv)>1 else Path.cwd()
portable=Path(sys.argv[2]) if len(sys.argv)>2 else None

def txt(rel): return (root/rel).read_text(encoding='utf-8-sig')
g=txt('src/Game.cpp'); h=txt('src/Game.h'); sem=txt('src/OpenBorSemantics.h'); cm=txt('CMakeLists.txt'); mw=txt('src/main_win.cpp')
checks={
 'cmake-version':'VERSION 0.3.32' in cm,
 'window-version':'Native C++ Port v0.3.32' in mw,
 'menu-version':'NATIVE PORT v0.3.32' in g,
 'blockodds-1n':'seed%(std::uint32_t)blockOdds' in sem and 'blockOdds<=0' in sem,
 'shootframe-default-zero':'e.kind=="shoot"){float h=e.args.empty()?0.f' in g,
 'atchain-hit-confirm':'comboAwaitingHit' in h and 'att.comboAwaitingHit&&!blocked' in g,
 'atchain-miss-reset':'starts(prev,"attack")&&a.player&&a.comboAwaitingHit' in g,
 'counter-not-melee-durability':'counter is the drop limit, not melee durability' in g,
 'shootnum-on-projectile':'if(firedWeapon&&a.player&&a.weaponNumber>0&&a.weaponUses>0)' in g,
 'weapon-drop-state':'weaponDropCount' in h and 'weaponItemModel' in h,
 'weapon-drop-runtime':'void Game::dropEquippedWeapon' in g and 't.weaponNumber>0)dropEquippedWeapon(t)' in g,
 'weapon-drop-limit':'nextDrop<=std::max(0,item->counter)' in g,
 'weapon-ground-counter':'s.mode=="item"' in g and 'a->weaponDropCount' in g,
 'weapon-swap-drop':'if(p.weaponNumber>0)dropEquippedWeapon(p)' in g,
 'new-level-no-weapon-carry':'old[i].w' not in g and 'struct PState{int hp=-1;}' in g,
 'save-v4':'version=4' in g and 'weapondrops' in g and 'weaponitem' in g,
}
if portable:
    data=portable/'assets'/'data'
    required=[
        'chars/axl/axl.txt','chars/slash/slash.txt','chars/edi.e/edi.e.txt',
        'chars/misc/pipe/pipe.txt','chars/misc/muramasa!/muramasa!.txt','chars/misc/knife/knife.txt'
    ]
    if data.exists() and all((data/rel).exists() for rel in required):
        def read_asset(rel): return (data/rel).read_text(encoding='latin-1').lower()
        checks['pak-axl-blockodds1']='blockodds 1' in ' '.join(read_asset('chars/axl/axl.txt').split())
        checks['pak-slash-blockodds1']='blockodds 1' in ' '.join(read_asset('chars/slash/slash.txt').split())
        checks['pak-edi-shootframe5']='shootframe 5' in ' '.join(read_asset('chars/edi.e/edi.e.txt').split())
        checks['pak-pipe-counter4']='counter 4' in ' '.join(read_asset('chars/misc/pipe/pipe.txt').split())
        checks['pak-muramasa-counter4']='counter 4' in ' '.join(read_asset('chars/misc/muramasa!/muramasa!.txt').split())
        checks['pak-knife-shootnum1']='shootnum 1' in ' '.join(read_asset('chars/misc/knife/knife.txt').split())
    else:
        print('pak_asset_signatures=SKIP complete PAK character assets unavailable in CI snapshot')
failed=[k for k,v in checks.items() if not v]
for k,v in checks.items(): print(f'{k}={"OK" if v else "FAIL"}')
if failed: raise SystemExit('v0.3.32 validation failed: '+', '.join(failed))
print('v0.3.32 PAK fidelity validator=OK')
