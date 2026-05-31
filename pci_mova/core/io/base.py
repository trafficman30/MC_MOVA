"""
pci_mova.core.io.base
Abstract IO interface — all IO implementations must satisfy this contract.
Physical hardware, simulation, and future API-driven IO all derive from here.
"""

from abc import ABC, abstractmethod
from pci_mova.core.model.buffers import KernelBuffers


class AbstractIO(ABC):
    """
    Defines the contract between the MOVA runtime and its IO layer.

    InputScan calls read_inputs()  → populates buffers.din / detsin / confin
    OutputScan calls write_outputs() → reads buffers.dout / control
    """

    @abstractmethod
    def read_inputs(self, buffers: KernelBuffers) -> None:
        """
        Read physical (or simulated) inputs into kernel buffers.
        Must populate:
          - buffers.detsin[]   (detector presence)
          - buffers.confin[]   (stage/phase confirms)
          - buffers.din[CRB]   (Controller Ready Bit)
        Must NOT modify outputs or trigger state transitions.
        """

    @abstractmethod
    def write_outputs(self, buffers: KernelBuffers) -> None:
        """
        Enforce kernel output decisions onto physical (or simulated) outputs.
        Reads:
          - buffers.dout[]     (stage forces, HI, TO, sync, fault LEDs)
          - buffers.control[]  (genstg output array)
        Safety always wins — this layer is the last line of defence.
        """

    @abstractmethod
    def snapshot(self) -> dict:
        """Return current IO state as a JSON-serialisable dict (for web API)."""

    def reset_confirms(self, stagon: int) -> None:
        """
        Initialise all confirm inputs to the inactive ("off") polarity for this dataset.
        stagon=0 (Open contact): inactive=1  stagon=1 (Closed contact): inactive=0
        Override in IO implementations that manage confirm state.
        """

    def set_intergreen_matrix(self, matrix: dict) -> None:
        """
        Provide the interstage intergreen lookup {(from, to): seconds}.
        Used by auto_follow mode to simulate correct TLC response timing.
        Override in IO implementations that support auto_follow.
        """
