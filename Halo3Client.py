"""Halo 3 Archipelago Client launcher."""
import sys
import ModuleUpdate
ModuleUpdate.update()

from worlds.halo3.client import launch

if __name__ == "__main__":
    launch(*sys.argv[1:])
