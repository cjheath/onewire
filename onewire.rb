require 'onewire.so'

module OneWire
    class Port
	attr_writer :devices
	attr_accessor :processing

	def devices
	    @devices || (@devices = enumerate)
	end
    end

    class Device
	attr_reader :serial

	def showSerial
	    # First byte is family code, last is CRC.
	    @serial[1..6].
		unpack("Z*")[0].    # Strip trailing \0's
		unpack("C*").	    # Break into unsigned characters
		collect { |c| "%02X" % c}.  # Convert to hex
		join		    # And join up
	end

	def familyCode
	    @serial[0]
	end

	def ==(other)
	    @serial == other.serial
	end
    end
end

