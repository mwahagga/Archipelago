from worlds.LauncherComponents import Component, Type, components, launch, SuffixIdentifier


def run_client(*args: str) -> None:
    from .client import launch as launch_client
    launch(launch_client, name="Halo 3 Client", args=args)


def launch_game_and_client(*args: str) -> None:
    from .client import launch as launch_client
    launch_args = (*args, "--launch")
    launch(launch_client, name="Halo 3 Client", args=launch_args)


components.append(
    Component(
        "Halo 3 Client",
        "Halo3Client",
        func=run_client,
        game_name="Halo 3",
        component_type=Type.CLIENT,
        supports_uri=True,
        file_identifier=SuffixIdentifier(".aphalo3"),
        description="Launch Halo 3 Archipelago Client or open a .aphalo3 patch file.",
    )
)

components.append(
    Component(
        "Halo 3 (Launch Game & Client)",
        func=launch_game_and_client,
        game_name="Halo 3",
        component_type=Type.CLIENT,
        description="Deploy mod, launch Halo: MCC with Anti-Cheat disabled, and start Halo 3 Archipelago Client.",
    )
)

