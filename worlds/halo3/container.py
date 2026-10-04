from __future__ import annotations
import json
import os
from typing import Any, ClassVar, Optional
import zipfile

from worlds.Files import APPlayerContainer


class Halo3Container(APPlayerContainer):
    """
    Container file for Halo 3 (.aphalo3).
    A zipfile containing archipelago.json metadata and slot configuration.
    """
    game: ClassVar[Optional[str]] = "Halo 3"
    patch_file_ending: str = ".aphalo3"

    def __init__(
        self,
        path: Optional[str] = None,
        player: Optional[int] = None,
        player_name: str = "",
        server: str = "",
        slot_data: Optional[dict[str, Any]] = None,
    ) -> None:
        super().__init__(path, player, player_name, server)
        self.slot_data = slot_data or {}

    def write_contents(self, opened_zipfile: zipfile.ZipFile) -> None:
        super().write_contents(opened_zipfile)
        if self.slot_data:
            opened_zipfile.writestr(
                "halo3_slot_data.json",
                json.dumps(self.slot_data, indent=2),
            )
