import os
from .cache import AdaptivePipelineCache

# Package metadata
__version__ = "0.1.5"
__author__ = "Nadav Keren"
__email__ = "nadavker@pm.me"
__description__ = "An adaptive Pipeline Cache composed of FIFO and upgraded TinyLFU with cost-awareness"

__all__ = [
    'AdaptivePipelineCache',
    'get_default_config_path',
]

def get_default_config_path() -> str:
    """Get the path to the bundled default configuration file.

    Returns:
        Path to the default config.json file
    """
    return os.path.join(os.path.dirname(__file__), 'config.json')

def create_cache(config_path: str) -> AdaptivePipelineCache:
    """Create an AdaptivePipelineCache from a configuration file.

    Args:
        config_path: Path to JSON configuration file

    Returns:
        AdaptivePipelineCache instance
    """
    return AdaptivePipelineCache(config_path)


def get_version() -> str:
    return __version__


def _check_cpp_extension():
    """Check if the C++ extension was built correctly."""
    try:
        from .adaptive_pipeline_impl import AdaptivePipelineCacheImpl
        return True
    except ImportError:
        return False

