from test.bases import WorldTestBase
from ..world import Halo3World


class Halo3TestBase(WorldTestBase):
    game = "Halo 3"
    world: Halo3World
