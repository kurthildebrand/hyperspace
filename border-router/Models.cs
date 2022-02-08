/************************************************************************************************//**
 * @file		Models.cs
 *
 * @copyright	Copyright 2022 Kurt Hildebrand.
 * @license		Licensed under the Apache License, Version 2.0 (the "License"); you may not use this
 *				file except in compliance with the License. You may obtain a copy of the License at
 *
 *				http://www.apache.org/licenses/LICENSE-2.0
 *
 *				Unless required by applicable law or agreed to in writing, software distributed under
 *				the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF
 *				ANY KIND, either express or implied. See the License for the specific language
 *				governing permissions and limitations under the License.
 *
 ***************************************************************************************************/
using NetTopologySuite.Geometries;
using System;
using System.ComponentModel.DataAnnotations.Schema;
using System.Net;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.IO;

namespace Hyperspace.Models
{
	public class IPv6Address : IPAddress, IComparable<IPv6Address>
	{
		// protected IPAddress address { get; set; }

		// public IPv6Address(byte[] address)
		// {
		// 	this.address = new IPAddress(address).MapToIPv6();
		// }
		// public IPv6Address(byte[] address, long scopeid)
		// {
		// 	this.address = new IPAddress(address, scopeid).MapToIPv6();
		// }
		// public IPv6Address(long newAddress)
		// {
		// 	this.address = new IPAddress(newAddress).MapToIPv6();
		// }
		// public IPv6Address(ReadOnlySpan<byte> address)
		// {
		// 	this.address = new IPAddress(address).MapToIPv6();
		// }
		// public IPv6Address(ReadOnlySpan<byte> address, long scopeid)
		// {
		// 	this.address = new IPAddress(address, scopeid).MapToIPv6();
		// }
		// public IPv6Address(string address)
		// {
		// 	this.address =  IPAddress.Parse(address).MapToIPv6();
		// }
		// public IPv6Address(IPAddress address)
		// {
		// 	this.address = address.MapToIPv6();
		// }

		public IPv6Address(byte[] address) : base(address) { }
		public IPv6Address(byte[] address, long scopeid) : base(address, scopeid) { }
		public IPv6Address(long newAddress) : base(newAddress) { }
		public IPv6Address(ReadOnlySpan<byte> address) : base(address) { }
		public IPv6Address(ReadOnlySpan<byte> address, long scopeid) : base(address, scopeid) { }
		public IPv6Address(string address) : base(IPAddress.Parse(address).GetAddressBytes()) { }
		public IPv6Address(IPAddress address) : base(address.GetAddressBytes()) { }

		public int CompareTo(IPv6Address that)
		{
			byte[] first  = this.GetAddressBytes();
			byte[] second = that.GetAddressBytes();

			if(first.Length < second.Length)
			{
				return -1;
			}
			else if(first.Length > second.Length)
			{
				return 1;
			}

			for(int i = 0; i < first.Length; i++)
			{
				if(first[i] < second[i])
				{
					return -1;
				}
				else if(first[i] > second[i])
				{
					return 1;
				}
			}

			return 0;
		}

		// public byte[] GetAddressBytes()
		// {
		// 	return address.GetAddressBytes();
		// }

		// public IPAddress ToIPAddress()
		// {
		// 	return address;
		// }

		// public override string ToString()
		// {
		// 	return address.ToString();
		// }
	}

	/* Todo: add this in .NET 6 */
	// public class RawJsonStringConverter : JsonConverter<string>
	// {
	// 	public override bool CanConvert(Type typeToConvert)
	// 	{
	// 		return typeToConvert == typeof(String);
	// 	}
	// 	public override string Read(
	// 		ref Utf8JsonReader reader,
	// 		Type typeToConvert,
	// 		JsonSerializerOptions options)
	// 	{
	// 		return reader.GetString();
	// 	}
	// 	public override void Write(
	// 		Utf8JsonWriter writer,
	// 		string value,
	// 		JsonSerializerOptions options)
	// 	{
	// 		writer.WriteRawValue(value);
	// 	}
	// }

	// public class HyperspaceCoordinate
	// {
	// 	/*    Column   |           Type           | Modifiers
	// 	 * ------------+--------------------------+-----------
	// 	 *  ip         | inet                     | not null
	// 	 *  updated_at | timestamp with time zone |
	// 	 *  r          | real                     |
	// 	 *  t          | real                     |
	// 	 *  seq        | integer                  | */
	// 	public string     ip         { get; set; }
	// 	public DateTime?  updated_at { get; set; }
	// 	public float?     r          { get; set; }
	// 	public float?     t          { get; set; }
	// 	public int?       seq        { get; set; }
	// }

	public class HyperspaceDevice
	{
		/*    Column        |           Type           | Modifiers
		 * -----------------+--------------------------+-----------
		 *  ip              | inet                     | not null
		 *  updated_at      | timestamp with time zone |
		 *  r               | real                     |
		 *  t               | real                     |
		 *  seq             | integer                  |
		 *  image-0         | text                     |
		 *  image-1         | text                     |
		 *  coap_well_known | text                     | */
		public string    ip              { get; set; }
		public DateTime? updated_at      { get; set; }
		public float?    r               { get; set; }
		public float?    t               { get; set; }
		public int?      seq             { get; set; }
		public string    image_0         { get; set; }
		public string    image_1         { get; set; }
		public string    coap_well_known { get; set; }
	}

	public class HyperspaceReport
	{
		/*    Column   |           Type           |          Modifiers
		 * ------------+--------------------------+------------------------------
		 *  ip         | inet                     |
		 *  updated_at | timestamp with time zone |
		 *  loc        | geometry(PointZ)         |
		 *  report     | jsonb                    | not null default '{}'::jsonb */
		public string   ip         { get; set; }
		public DateTime updated_at { get; set; }
		public Point    loc        { get; set; }
		public Dictionary<string, JsonElement> report { get; set; }

		public virtual HyperspaceDevice? IpNavigation { get; set; }
	}

	public class DbNotification
	{
		public string table  { get; set; }
		public string action { get; set; }
		public object data   { get; set; }
	}

	public class FirmwareString : IComparable<FirmwareString>
	{
		public string  Manuf   { get; private set; }
		public string  Board   { get; private set; }
		public Version Version { get; private set; }

		/* Warning: Expects no file extension */
		public FirmwareString(string firmware_string)
		{
			firmware_string = Path.GetFileName(firmware_string);

			var parts = firmware_string.Split(",");
			Manuf   = parts[0];
			Board   = parts[1];
			Version = new Version(parts[2]);
		}

		public override string ToString()
		{
			return $"{Manuf},{Board},{Version}";
		}

		public int CompareTo(FirmwareString other)
		{
			int c;

			c = Manuf.CompareTo(other.Manuf);

			if(c != 0)
			{
				return c;
			}

			c = Board.CompareTo(other.Board);

			if(c != 0)
			{
				return c;
			}

			c = Version.CompareTo(other.Version);

			return c;
		}
	}
}
