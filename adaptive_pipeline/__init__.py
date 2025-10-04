from .cache import AdaptivePipelineCache

# Package metadata
__version__ = "0.1.0"
__author__ = "Nadav Keren"
__email__ = "nadavker@pm.me"
__description__ = "Test for creating pip packages"

__all__ = [
    'AdaptivePipelineCache',
]

def create_cache(maxsize: int) -> AdaptivePipelineCache:
    return AdaptivePipelineCache(maxsize)


def get_version() -> str:
    return __version__


def _check_cpp_extension():
    """Check if the C++ extension was built correctly."""
    try:
        from .adaptive_pipeline_impl import AdaptivePipelineCacheImpl
        return True
    except ImportError:
        return False

