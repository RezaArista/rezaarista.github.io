
"use client";

import { useState, useEffect, useCallback } from 'react';

type SetValue<T> = (value: T | ((val: T) => T)) => void;

function useLocalStorage<T>(key: string, initialValue: T): [T, SetValue<T>] {
  // State to store our value
  // Start with initialValue to prevent hydration mismatch during server render and initial client render
  const [storedValue, setStoredValue] = useState<T>(initialValue);

  // Effect to load the value from localStorage when the component mounts on the client
  useEffect(() => {
    // This check ensures we are on the client side and this effect runs after initial render
    if (typeof window !== 'undefined') {
      try {
        const item = window.localStorage.getItem(key);
        // Parse stored json or if none, the state remains initialValue
        if (item !== null) {
          setStoredValue(JSON.parse(item) as T);
        }
      } catch (error) {
        // If error, state remains initialValue
        console.warn(`Error reading localStorage key "${key}" on client mount:`, error);
      }
    }
  }, [key]); // Only re-run if key changes (which is typically stable for a given hook instance)

  const setValue: SetValue<T> = useCallback(
    (value) => {
      try {
        // Allow value to be a function so we have the same API as useState
        const valueToStore =
          value instanceof Function ? value(storedValue) : value;
        // Save state
        setStoredValue(valueToStore);
        // Save to local storage
        if (typeof window !== 'undefined') {
          window.localStorage.setItem(key, JSON.stringify(valueToStore));
          // Dispatch a custom event to notify other instances of this hook on the same page
          window.dispatchEvent(new CustomEvent('local-storage-updated', { detail: { key } }));
        }
      } catch (error) {
        console.warn(`Error setting localStorage key "${key}":`, error);
      }
    },
    [key, storedValue] // storedValue is needed if `value` is a function
  );

  // Effect to listen for changes from other tabs/windows (via 'storage' event)
  // and from other instances of this hook on the same page (via 'local-storage-updated' custom event)
  useEffect(() => {
    const handleStorageChange = (event: StorageEvent) => {
      if (event.key === key && event.storageArea === window.localStorage) {
        if (event.newValue !== null) {
          try {
            setStoredValue(JSON.parse(event.newValue) as T);
          } catch (error) {
            console.warn(`Error parsing storage change for key "${key}":`, error);
            setStoredValue(initialValue); // Fallback to initialValue on error
          }
        } else {
          // Item was removed from localStorage in another tab
          setStoredValue(initialValue);
        }
      }
    };

    // Custom event listener for same-page updates
    const handleLocalStorageUpdated = (event: Event) => {
      const customEvent = event as CustomEvent;
      if (customEvent.detail && customEvent.detail.key === key) {
        if (typeof window !== 'undefined') {
            try {
                const item = window.localStorage.getItem(key);
                if (item !== null) {
                    setStoredValue(JSON.parse(item) as T);
                } else {
                    setStoredValue(initialValue);
                }
            } catch (error) {
                console.warn(`Error re-reading localStorage key "${key}" on update event:`, error);
                setStoredValue(initialValue);
            }
        }
      }
    };

    if (typeof window !== 'undefined') {
      window.addEventListener('storage', handleStorageChange);
      window.addEventListener('local-storage-updated', handleLocalStorageUpdated);
    }

    return () => {
      if (typeof window !== 'undefined') {
        window.removeEventListener('storage', handleStorageChange);
        window.removeEventListener('local-storage-updated', handleLocalStorageUpdated);
      }
    };
  }, [key, initialValue]); // initialValue is needed for fallback on error or item removal

  return [storedValue, setValue];
}

export default useLocalStorage;
