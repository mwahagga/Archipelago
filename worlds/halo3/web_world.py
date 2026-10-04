from BaseClasses import Tutorial
from worlds.AutoWorld import WebWorld
from .options import option_groups, options_presets


class Halo3WebWorld(WebWorld):
    game = "Halo 3"
    theme = "partyTime"
    option_groups = option_groups
    options_presets = options_presets

    tutorials = [
        Tutorial(
            tutorial_name="Setup Guide",
            description="A guide to setting up Halo 3 randomizer.",
            language="English",
            file_name="setup_en.md",
            link="setup/en",
            authors=["Mwahagga"],
        )
    ]

