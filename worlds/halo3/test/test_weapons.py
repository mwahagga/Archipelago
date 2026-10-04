from .bases import Halo3TestBase
from ..items import INDIVIDUAL_WEAPON_LICENSES, TIER_WEAPON_LICENSES


class TestVanillaWeapons(Halo3TestBase):
    options = {
        "weapon_license_mode": 0,  # Disabled / Vanilla
    }

    def test_no_weapon_licenses_in_pool(self) -> None:
        """When weapon licenses are disabled, no weapon licenses should exist in item pool."""
        pool_names = [item.name for item in self.multiworld.itempool]
        for license_name in INDIVIDUAL_WEAPON_LICENSES:
            self.assertNotIn(license_name, pool_names)
        for tier_name in TIER_WEAPON_LICENSES:
            self.assertNotIn(tier_name, pool_names)

    def test_no_weapon_licenses_precollected(self) -> None:
        """No weapon licenses should be precollected in vanilla mode."""
        precollected = [item.name for item in self.multiworld.precollected_items[self.player]]
        for license_name in INDIVIDUAL_WEAPON_LICENSES:
            self.assertNotIn(license_name, precollected)


class TestIndividualWeapons(Halo3TestBase):
    options = {
        "weapon_license_mode": 1,   # Individual
        "starting_weapon": 1,       # Magnum (+ guaranteed Plasma Pistol)
    }

    def test_configured_starting_weapon_precollected(self) -> None:
        """Configured starting weapon (Magnum) and guaranteed sidearm (Plasma Pistol) should be precollected."""
        precollected = [item.name for item in self.multiworld.precollected_items[self.player]]
        self.assertIn("Magnum License", precollected)
        self.assertIn("Plasma Pistol License", precollected)

    def test_remaining_licenses_in_pool(self) -> None:
        """All 17 non-starter licenses must be present in itempool."""
        pool_names = [item.name for item in self.multiworld.itempool]
        self.assertNotIn("Magnum License", pool_names)
        self.assertNotIn("Plasma Pistol License", pool_names)
        for license_name in INDIVIDUAL_WEAPON_LICENSES:
            if license_name not in ("Magnum License", "Plasma Pistol License"):
                self.assertIn(license_name, pool_names)


class TestTierWeapons(Halo3TestBase):
    options = {
        "weapon_license_mode": 2,  # Tiers
    }

    def test_starter_tier_precollected(self) -> None:
        """Starter tier 'Sidearms & Light Automatics License' must be precollected."""
        precollected = [item.name for item in self.multiworld.precollected_items[self.player]]
        self.assertIn("Sidearms & Light Automatics License", precollected)

    def test_remaining_tier_licenses_in_pool(self) -> None:
        """The other 3 tier licenses must be in item pool."""
        pool_names = [item.name for item in self.multiworld.itempool]
        self.assertIn("Precision Rifles License", pool_names)
        self.assertIn("Close-Quarters & Assault License", pool_names)
        self.assertIn("Power Weapons License", pool_names)
        self.assertNotIn("Sidearms & Light Automatics License", pool_names)

    def test_no_individual_licenses_in_tier_mode(self) -> None:
        """Individual weapon licenses should not appear in pool during tier mode."""
        pool_names = [item.name for item in self.multiworld.itempool]
        for license_name in INDIVIDUAL_WEAPON_LICENSES:
            self.assertNotIn(license_name, pool_names)


class TestItemPoolBalance(Halo3TestBase):
    options = {
        "weapon_license_mode": 1,
        "include_skulls": 1,
        "include_terminals": 1,
        "include_rally_points": 1,
    }

    def test_itempool_exact_match_with_unfilled_locations(self) -> None:
        """Multiworld itempool must exactly match unfilled location count."""
        unfilled = self.multiworld.get_unfilled_locations(self.player)
        self.assertEqual(len(self.multiworld.itempool), len(unfilled))
