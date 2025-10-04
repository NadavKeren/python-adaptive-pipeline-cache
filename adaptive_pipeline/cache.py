"""
FIFO Cache implementation with C++ backend.
This module provides the Python interface to the C++ implementation.
"""

import collections.abc
from typing import Tuple, Iterator, Optional, Any

try:
    from ._adaptive_pipeline_cache_impl import AdaptivePipelineCacheImpl
except ImportError as e:
    raise ImportError(
        "Could not import C++ extension. Make sure the package was built correctly. "
        f"Original error: {e}"
    ) from e


class AdaptivePipelineCache(collections.abc.MutableMapping):
    
    __marker : Tuple[float, int] = (-1.0, -1)  # Sentinel for default values
    
    def __init__(self, maxsize: int):
        if not isinstance(maxsize, int) or maxsize <= 0:
            raise ValueError("maxsize must be a positive integer")
            
        self._impl = AdaptivePipelineCacheImpl(maxsize)
        
        self.__size = 0
    
    def __repr__(self) -> str:
        return repr(AdaptivePipelineCacheImpl)
    
    def __getitem__(self, key: int) -> Tuple[float, int]:
        self._validate_key(key)
        return self._impl[key]
    
    def __setitem__(self, key: int, value: Tuple[float, int]) -> None:
        """
        Set item with key and value.
        
        Args:
            key: Non-negative integer key
            value: Tuple of (latency, data) where latency is float and data is int
            
        Raises:
            ValueError: If key or value format is invalid
        """
        self._validate_key(key)
        self._validate_value(value)
        
        # Convert to ensure correct types
        latency, data = value
        self._impl[key] = (float(latency), int(data))
    
    def __delitem__(self, key: int) -> None:
        """
        Delete item by key.
        
        Args:
            key: Key to delete
            
        Raises:
            KeyError: If key is not found
        """
        self._validate_key(key)
        del self._impl[key]
    
    def __contains__(self, key: int) -> bool:
        """Check if key exists in cache."""
        if not isinstance(key, int) or key < 0:
            return False
        return key in self._impl
    
    def __missing__(self, key: int) -> None:
        """Handle missing key (raises KeyError)."""
        raise KeyError(key)
    
    def __iter__(self) -> Iterator[int]:
        """Iterate over keys."""
        return iter(self._impl.keys())
    
    def __len__(self) -> int:
        """Get number of items in cache."""
        return len(self._impl)
    
    def get(self, key: int, default: Optional[Tuple[float, int]] = None) -> Tuple[float, int]:
        """
        Get item with default value.
        
        Args:
            key: Key to look up
            default: Value to return if key is not found
            
        Returns:
            Value for key or default if key not found
        """
        if key in self:
            return self[key]
        return default if default is not None else (0.0, 0)
    
    def pop(self, key: int, default: Tuple[float, int]=__marker) -> Tuple[float, int]:
        """
        Remove and return item.
        
        Args:
            key: Key to remove
            default: Value to return if key is not found
            
        Returns:
            Value that was removed
            
        Raises:
            KeyError: If key is not found and no default provided
        """
        if key in self:
            value = self[key]
            del self[key]
            return value
        elif default is not self.__marker:
            return default
        else:
            raise KeyError(key)
    
    def popitem(self) -> Tuple[int, Tuple[float, int]]:
        """
        Remove and return oldest item (FIFO behavior).
        
        Returns:
            Tuple of (key, value) for the oldest item
            
        Raises:
            KeyError: If cache is empty
        """
        return self._impl.popitem()
    
    def clear(self) -> None:
        """Remove all items from cache."""
        self._impl.clear()
    
    def keys(self):
        """Get view of all keys."""
        return self._impl.keys()
    
    def values(self):
        """Get view of all values."""
        return self._impl.values()
    
    def items(self):
        """Get view of all key-value pairs."""
        return [(k, self._impl[k]) for k in self._impl.keys()]
    
    @property
    def maxsize(self) -> int:
        """The maximum size of the cache."""
        return self._impl.maxsize
    
    @property
    def currsize(self) -> int:
        """The current size of the cache (same as len() for this implementation)."""
        return self._impl.currsize
    
    def _validate_key(self, key: Any) -> None:
        if not isinstance(key, int) or key < 0:
            raise ValueError("Key must be a non-negative integer")
    
    def _validate_value(self, value: Any) -> None:
        if not isinstance(value, tuple) or len(value) != 2:
            raise ValueError("Value must be a tuple of (latency: float, num_of_tokens: int)")
        
        latency, num_of_tokens = value
        if not isinstance(latency, (int, float)):
            raise ValueError("Latency must be a number (int or float)")

        if not isinstance(num_of_tokens, int) or num_of_tokens < 0:
            raise ValueError("num_of_tokens must be a non-negative integer")

    @staticmethod
    def _default_getsizeof(value: Tuple[float, int]) -> int:
        """Default size calculation (each item has size 1)."""
        return 1



__all__ = ['AdaptivePipeLineCache']
