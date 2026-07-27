# API reference

## Synth

```{eval-rst}
.. autoclass:: vita.Synth
   :members:
   :undoc-members:
   :special-members: __init__
```

## ControlValue

A live handle to one of a synth's controls, obtained from
{py:meth}`vita.Synth.get_controls`.

```{eval-rst}
.. autoclass:: vita.vita.ControlValue
   :members:
   :undoc-members:
```

## ControlInfo

Metadata describing a control's range and presentation, returned by
{py:meth}`vita.Synth.get_control_details`.

```{eval-rst}
.. autoclass:: vita.vita.ControlInfo
   :members:
   :undoc-members:
```

## Module functions

```{eval-rst}
.. autofunction:: vita.get_modulation_sources

.. autofunction:: vita.get_modulation_destinations
```

## Constants

`vita.constants` holds the named values for discrete controls.

```{eval-rst}
.. automodule:: vita.constants
   :members:
   :undoc-members:
   :no-index:
```
