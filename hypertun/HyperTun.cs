/************************************************************************************************//**
 * @file		tun_test.c
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
 * @brief
 * @desc		To create a new interface, with the IP address "fd00::1/8" and accessible by the
 * 				current user:
 *
 * 				$ sudo ip tuntap add mode tun user $USER
 * 				$ sudo ip link set tun0 up
 * 				$ sudo ip addr add "fd00::1/8" dev tun0
 * 				$ sudo radvd
 * 				$ ip tuntap
 * 				$ sudo tcpdump -vv -i tun0
 * 				$ sudo tcpdump -nnvvXSs 1514 -i tun0
 *
 * 				/etc/radvd.conf
 * 				interface tun0 {
 * 					AdvSendAdvert on;
 * 					MinRtrAdvInterval 3;
 * 					MaxRtrAdvInterval 10;
 * 					prefix fd00::/64 {
 * 						AdvOnLink on;
 * 						AdvAutonomous on;
 * 						AdvRouterAddr on;
 * 					};
 * 				};
 *
 *				stop radvd: sudo killall radvd
 */
using System;
using System.Net;
using System.Threading;
using System.Threading.Tasks;
using System.Runtime.InteropServices;
using Npgsql;
using Microsoft.Extensions.Logging;

namespace Hyperspace
{
	using sa_family_t = UInt16;
	using in_addr_t   = UInt32;
	using in_port_t   = UInt16;
	using off_t       = Int64;

	class Program
	{
		static async Task Main(string[] args)
		{
			CancellationTokenSource source = new CancellationTokenSource();

			using var loggerFactory = LoggerFactory.Create(builder =>
			{
				builder
					.AddFilter("Hyperspace.HyperTun", LogLevel.Debug)
					.AddSimpleConsole(c =>
					{
						c.ColorBehavior   = Microsoft.Extensions.Logging.Console.LoggerColorBehavior.Enabled;
						c.SingleLine      = true;
						c.TimestampFormat = "[yyyy-MM-dd HH:mm:ss.ffffff] ";
					});
			});

			HyperTun tun = new HyperTun(
				loggerFactory.CreateLogger<HyperTun>(),
				"Host=localhost;Username=pi;Password=justtryit;Database=hyperspace;");

			tun.Setup();

			await tun.Run(source.Token);
		}
	}

	public class HyperTun
	{
		private uint   spi_mode  = SPI_CPOL | SPI_CPHA;
		private byte   spi_bits  = 0;
		private UInt16 spi_delay = 0;
		private uint   spi_speed = 8000000;
		private string tun_name  = "tun0";

		private nint spifd;
		private nint readyfd;
		private nint tunfd;

		private ILogger<HyperTun> logger;
		private string connection;
		private readonly object spilock = new object();

		public HyperTun(ILogger<HyperTun> logger, string connection)
		{
			this.logger     = logger;
			this.connection = connection;
		}

		public void Setup()
		{
			logger.LogInformation("Setting up HyperTun");

			spifd   = SetupSpim();
			readyfd = SetupReadyPin();
			tunfd   = SetupTun();
		}

		public Task Run(CancellationToken token)
		{
			logger.LogInformation("Running HyperTun");

			return Task.Run(() => Loop(token));
		}

		private nint SetupSpim()
		{
			nint ret, fd;

			unsafe
			{
				fd = open("/dev/spidev0.0", O_RDWR);
				if(fd < 0)
				{
					/* error: open /dev/spidev0.0 */
					return fd;
				}

				fixed(uint* mode = &spi_mode)
				{
					/* Set SPI Mode */
					ret = ioctl(fd, SPI_IOC_WR_MODE, mode);
					if(ret == -1)
					{
						// perror("can't set spi mode");
						return ret;
					}

					ret = ioctl(fd, SPI_IOC_RD_MODE, mode);
					if(ret == -1)
					{
						// perror("can't set spi read mode");
						return ret;
					}
				}

				fixed(byte* bits = &spi_bits)
				{
					/* Set bits per word */
					ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, bits);
					if(ret == -1)
					{
						// perror("can't set spi write bits per word");
						return ret;
					}

					ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, bits);
					if(ret == -1)
					{
						// perror("can't set spi read bits per word");
						return ret;
					}
				}

				fixed(uint* speed = &spi_speed)
				{
					/* Set max speed in Hz */
					ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, speed);
					if(ret == -1)
					{
						// perror("can't set spi write speed");
						return ret;
					}

					ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, speed);
					if(ret == -1)
					{
						// perror("can't set spi read speed");
						return ret;
					}
				}
			}

			return fd;
		}

		private nint SetupReadyPin()
		{
			nint ret = 0, fd = 0;

			unsafe
			{
				/* Export GPIO 22 (Header Pin 15) */
				fd = open("/sys/class/gpio/export", O_WRONLY);
				if(fd > 0)
				{
					ret = write(fd, "22", 2);
					close(fd);
				}

				/* Set direction to in */
				fd = open("/sys/class/gpio/gpio22/direction", O_RDWR);
				if(fd < 0)
				{
					// perror("open direction failed");
					return fd;
				}

				ret = write(fd, "in", 2);
				if(ret != 2)
				{
					// perror("set dir to in failed");
					close(fd);
					return ret;
				}

				close(fd);

				/* Enable rising edge interrupt on GPIO 22 (Header Pin 15). This signal is active high and
				* signals that the hat has a packet to transmit to the Raspberry Pi. */
				fd = open("/sys/class/gpio/gpio22/edge", O_RDWR);
				if(fd < 0)
				{
					// perror("open edge failed");
					return fd;
				}

				ret = write(fd, "rising", 6);
				if(ret != 6)
				{
					// perror("set edge irq failed");
					close(fd);
					return ret;
				}

				close(fd);

				/* Open GPIO 22 (Header pin 15) value for reading. Read character '0' for low,
				 * character '1' for high. */
				return open("/sys/class/gpio/gpio22/value", O_RDONLY);
			}
		}

		private nint SetupTun()
		{
			ifreq ifr = new ifreq();

			nint fd, err;

			unsafe
			{
				/* Open the tun device. Return error if it doesn't exist. */
				if((fd = open("/dev/net/tun", O_RDWR)) < 0)
				{
					// perror("open /dev/net/tun");
					return fd;
				}

				/* Set ifreq flags and device name */
				ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
				ifr.ifr_name  = tun_name;

				/* Create the new device */
				if((err = ioctl(fd, TUNSETIFF, ref ifr)) < 0)
				{
					// perror("ioctl TUNSETIFF");
					close(fd);
					return err;
				}

				// /* Copy the device name out to the caller and return the file descriptor. */
				// strcpy(devname, ifr.ifr_name);
				return fd;
			}
		}

		private async Task Loop(CancellationToken token)
		{
			nint   ret;
			byte[] value = new byte[1];
			var    txbuf = new PinnedBuffer(2048);	/* Transmitting to SPI */
			var    rxbuf = new PinnedBuffer(2048);	/* Receiving from SPI */
			IntPtr pkt;

			unsafe
			{
				pkt = hyperopt_alloc_pkt();
			}

			/* Setup connection to the database */
			await using var db = new NpgsqlConnection(connection);
			await db.OpenAsync();

			var events = new pollfd[2]
			{
				new pollfd { events = (UInt16)(POLLPRI | POLLIN),  fd = tunfd,   },
				new pollfd { events = (UInt16)(POLLPRI | POLLERR), fd = readyfd, },
			};

			while(!token.IsCancellationRequested)
			{
				/* Reset poll events */
				events[0].revents = 0;
				events[1].revents = 0;

				ret = Poll(events, 2, token);

				if(ret < 0)
				{
					return;
				}

				nint txcount = 0;
				nint rxcount = 0;

				/* Handle data to transmit to the SPI device */
				if(events[0].revents != 0)
				{
					txcount = await HandleTxData(pkt, txbuf, db, token);
				}

				/* Simultaneously transmit/receive data to/from the SPI device */
				unsafe
				{
					rxcount = SpiTrx(txbuf, txcount, rxbuf, (nint)rxbuf.Length);
				}

				/* Handle any data received from the SPI device */
				if(events[1].revents != 0)
				{
					await HandleRxData(pkt, rxbuf, rxcount, db, token);
				}
			}
		}

		private nint Poll(pollfd[] fds, UInt32 nfds, CancellationToken token)
		{
			nint   ret;
			byte[] value = new byte[1];

			while(!token.IsCancellationRequested)
			{
				/* If ready pin is high, then new data from SPI to TUN */
				ret = (nint)lseek(readyfd, 0, SEEK_SET);
				if(ret < 0)
				{
					logger.LogError("can't lseek gpio value");
					// close(params->ready_fd);
					return -EIO;
				}

				ret = read(readyfd, value, 1);
				if(ret < 0)
				{
					logger.LogError("can't read gpio value");
					//close(params->ready_fd);
					return -EIO;
				}

				if(value[0] == '1')
				{
					return 0;
				}

				/* Poll for GPIO interrupt or new data from TUN to SPI */
				ret = poll(fds, 2, 1000);
				if(ret < 0)
				{
					logger.LogError("poll failed");
					//close(params->ready_fd);
					return -EIO;
				}
				else if(ret > 0)
				{
					return ret;
				}
			}

			return -ECANCELED;
		}

		private async Task<nint> HandleTxData(
			IntPtr            pkt,
			PinnedBuffer      txbuf,
			NpgsqlConnection  db,
			CancellationToken token)
		{
			IntPtr opt;

			nint txcount = read(tunfd, txbuf.Data, txbuf.Length);

			if(txcount < 0)
			{
				logger.LogError("Could not read from tun!");
				return -EIO;
			}

			var src  = new IPAddress(new ReadOnlySpan<byte>(txbuf.Data, 8, 16));
			var dest = new IPAddress(new ReadOnlySpan<byte>(txbuf.Data, 24, 16));

			unsafe
			{
				txcount = hyperopt_insert(pkt, txbuf.Data, txcount, txbuf.Length, out opt);
			}

			/* Get the destination coordinates from the database */
			await using(var cmd = new NpgsqlCommand("SELECT * FROM devices WHERE ip = @ip", db))
			{
				cmd.Parameters.AddWithValue("ip", dest);

				await using(var reader = await cmd.ExecuteReaderAsync(token))
				{
					while(await reader.ReadAsync())
					{
						/* Fill in the destination coordinates in the packet */
						float r   = Convert.ToSingle(reader["r"]);
						float t   = Convert.ToSingle(reader["t"]);
						byte  seq = Convert.ToByte(reader["seq"]);

						unsafe
						{
							hyperopt_set(opt, r, t, seq);
						}
					}
				}
			}

			unsafe
			{
				hyperopt_finalize(pkt);
			}

			logger.LogDebug("host {0,-39} -> node {1,-39} len {2}", src, dest, txcount);

			return txcount;
		}

		private async Task<nint> HandleRxData(
			IntPtr            pkt,
			PinnedBuffer      rxbuf,
			nint              rxcount,
			NpgsqlConnection  db,
			CancellationToken token)
		{
			if(rxcount <= 0)
			{
				return -EIO;
			}

			IntPtr opt;

			var src  = new IPAddress(new ReadOnlySpan<byte>(rxbuf.Data, 8, 16));
			var dest = new IPAddress(new ReadOnlySpan<byte>(rxbuf.Data, 24, 16));

			unsafe
			{
				rxcount = hyperopt_get(pkt, rxbuf.Data, rxcount, rxbuf.Length, out opt);
			}

			if(opt != IntPtr.Zero && !src.Equals(IPAddress.IPv6Any))
			{
				float r;
				float t;
				byte  seq;

				unsafe
				{
					r   = hyperopt_src_r(opt);
					t   = hyperopt_src_t(opt);
					seq = hyperopt_src_seq(opt);
				}

				await using(var cmd = new NpgsqlCommand(
					"INSERT INTO devices (ip, updated_at, r, t, seq) " +
					"VALUES(@ip, CURRENT_TIMESTAMP, @r, @t, @seq) " +
					"ON CONFLICT (ip) " +
					"DO UPDATE " +
					"    SET updated_at = excluded.updated_at, " +
					"        r   = excluded.r, " +
					"        t   = excluded.t, " +
					"        seq = excluded.seq", db))
				{
					cmd.Parameters.AddWithValue("ip",  src);
					cmd.Parameters.AddWithValue("r",   r);
					cmd.Parameters.AddWithValue("t",   t);
					cmd.Parameters.AddWithValue("seq", seq);
					await cmd.ExecuteNonQueryAsync();
				}
			}

			logger.LogDebug("host {0,-39} <- node {1,-39} len {2}", dest, src, rxcount);

			unsafe
			{
				nint ret = write(tunfd, rxbuf.Data, rxcount);

				if(ret < 0)
				{
					logger.LogError("write error {0}", ret);
					return -EIO;
				}
			}

			return 0;
		}

		/* SpiTrx *******************************************************************************//**
		 * @brief		*/
		private unsafe nint SpiTrx(PinnedBuffer txbuf, nint txlen, PinnedBuffer rxbuf, nint rxlen)
		{
			nint ret;

			lock(spilock)
			{
				/* Guarantee that txlen must be at least 40 bytes */
				if(txlen < 40)
				{
					txlen = 0;
				}

				var trx1 = new spi_ioc_transfer
				{
					tx_buf        = txlen != 0 ? (UInt64)txbuf.Ptr : 0,
					rx_buf        = (UInt64)rxbuf.Ptr,
					len           = 40,
					speed_hz      = spi_speed,
					delay_usecs   = spi_delay,
					bits_per_word = spi_bits,
					cs_change     = 1,
				};

				ret = ioctl(spifd, SPI_IOC_MESSAGE(1), &trx1);

				/* Compute the length of the rx spi packet if it exists. First check that the spi
				 * device is transmitting a packet to us by checking the version. It will be 6 if it
				 * is transmitting. */
				uint version = (uint)((rxbuf[0] & 0xF0) >> 4);
				uint rxcount = version == 6u ? (uint)(rxbuf[4] << 8) | (uint)(rxbuf[5] << 0) : 0;

				/* Check if rxcount is larger than rx buffer capacity */
				if(rxcount > rxlen)
				{
					rxcount = 0;
				}

				/* Update txlen to reflect the fact that 40 bytes have already been transmitted */
				if(txlen >= 40)
				{
					txlen -= 40;
				}

				/* Compute the length of the final spi transfer. The length is the maximum of the
				* remaining tx packet length and rx packet length. */
				uint remaining = txlen > rxcount ? (uint)txlen : rxcount;

				/* If both tx len is 0 (we are not transmitting a packet) and rxcount is 0 (spi slave
				 * is not transmitting a packet), then cancel the spi transfer. */
				if(remaining == 0)
				{
					var trx2 = new spi_ioc_transfer
					{
						tx_buf        = 0,
						rx_buf        = 0,
						len           = 0,
						delay_usecs   = 0,
						speed_hz      = spi_speed,
						bits_per_word = spi_bits,
						cs_change     = 0,
					};

					ioctl(spifd, SPI_IOC_MESSAGE(1), &trx2);
					ret = -EIO;
				}
				else
				{
					var trx2 = new spi_ioc_transfer
					{
						tx_buf        = txlen != 0 ? (UInt64)txbuf.Ptr + 40 : 0,
						rx_buf        = rxlen != 0 ? (UInt64)rxbuf.Ptr + 40 : 0,
						len           = remaining,
						delay_usecs   = 0,
						speed_hz      = spi_speed,
						bits_per_word = spi_bits,
						cs_change     = 0,
					};

					ret  = ioctl(spifd, SPI_IOC_MESSAGE(1), &trx2);
					ret += 40;
				}
			}

			return ret;
		}

		#region Native Hyperopt
		[DllImport("libhyperopt", SetLastError = true)]
		public unsafe static extern IntPtr hyperopt_alloc_pkt();

		[DllImport("libhyperopt", SetLastError = true)]
		public unsafe static extern void hyperopt_free_pkt(IntPtr pkt);

		[DllImport("libhyperopt", SetLastError = true)]
		public unsafe static extern nint hyperopt_insert(IntPtr pkt, byte[] buffer, nint count, nuint size, out IntPtr opt);

		[DllImport("libhyperopt", SetLastError = true)]
		public unsafe static extern nint hyperopt_get(IntPtr pkt, byte[] buffer, nint count, nuint size, out IntPtr opt);

		[DllImport("libhyperopt", SetLastError = true)]
		public unsafe static extern float hyperopt_src_r(IntPtr opt);

		[DllImport("libhyperopt", SetLastError = true)]
		public unsafe static extern float hyperopt_src_t(IntPtr opt);

		[DllImport("libhyperopt", SetLastError = true)]
		public unsafe static extern byte hyperopt_src_seq(IntPtr opt);

		[DllImport("libhyperopt", SetLastError = true)]
		public unsafe static extern void hyperopt_set(IntPtr opt, float r, float t, byte dest_seq);

		[DllImport("libhyperopt", SetLastError = true)]
		public unsafe static extern void hyperopt_finalize(IntPtr pkt);
		#endregion
		#region Errno
		internal const nint EPERM =           1;
		internal const nint ENOENT =          2;
		internal const nint ESRCH =           3;
		internal const nint EINTR =           4;
		internal const nint EIO =             5;
		internal const nint ENXIO =           6;
		internal const nint E2BIG =           7;
		internal const nint ENOEXEC =         8;
		internal const nint EBADF =           9;
		internal const nint ECHILD =          10;
		internal const nint EAGAIN =          11;
		internal const nint ENOMEM =          12;
		internal const nint EACCES =          13;
		internal const nint EFAULT =          14;
		internal const nint EBUSY =           16;
		internal const nint EEXIST =          17;
		internal const nint EXDEV =           18;
		internal const nint ENODEV =          19;
		internal const nint ENOTDIR =         20;
		internal const nint EISDIR =          21;
		internal const nint ENFILE =          23;
		internal const nint EMFILE =          24;
		internal const nint ENOTTY =          25;
		internal const nint EFBIG =           27;
		internal const nint ENOSPC =          28;
		internal const nint ESPIPE =          29;
		internal const nint EROFS =           30;
		internal const nint EMLINK =          31;
		internal const nint EPIPE =           32;
		internal const nint EDOM =            33;
		internal const nint EDEADLK =         36;
		internal const nint ENAMETOOLONG =    38;
		internal const nint ENOLCK =          39;
		internal const nint ENOSYS =          40;
		internal const nint ENOTEMPTY =       41;

		internal const nint EINVAL =          22;
		internal const nint ERANGE =          34;
		internal const nint EILSEQ =          42;
		internal const nint STRUNCATE =       80;

		internal const nint EDEADLOCK =       EDEADLK;
		internal const nint EADDRINUSE =      100;
		internal const nint EADDRNOTAVAIL =   101;
		internal const nint EAFNOSUPPORT =    102;
		internal const nint EALREADY =        103;
		internal const nint EBADMSG =         104;
		internal const nint ECANCELED =       105;
		internal const nint ECONNABORTED =    106;
		internal const nint ECONNREFUSED =    107;
		internal const nint ECONNRESET =      108;
		internal const nint EDESTADDRREQ =    109;
		internal const nint EHOSTUNREACH =    110;
		internal const nint EIDRM =           111;
		internal const nint EINPROGRESS =     112;
		internal const nint EISCONN =         113;
		internal const nint ELOOP =           114;
		internal const nint EMSGSIZE =        115;
		internal const nint ENETDOWN =        116;
		internal const nint ENETRESET =       117;
		internal const nint ENETUNREACH =     118;
		internal const nint ENOBUFS =         119;
		internal const nint ENODATA =         120;
		internal const nint ENOLINK =         121;
		internal const nint ENOMSG =          122;
		internal const nint ENOPROTOOPT =     123;
		internal const nint ENOSR =           124;
		internal const nint ENOSTR =          125;
		internal const nint ENOTCONN =        126;
		internal const nint ENOTRECOVERABLE = 127;
		internal const nint ENOTSOCK =        128;
		internal const nint ENOTSUP =         129;
		internal const nint EOPNOTSUPP =      130;
		internal const nint EOTHER =          131;
		internal const nint EOVERFLOW =       132;
		internal const nint EOWNERDEAD =      133;
		internal const nint EPROTO =          134;
		internal const nint EPROTONOSUPPORT = 135;
		internal const nint EPROTOTYPE =      136;
		internal const nint ETIME =           137;
		internal const nint ETIMEDOUT =       138;
		internal const nint ETXTBSY =         139;
		internal const nint EWOULDBLOCK =     140;
		#endregion
		#region Syscalls
		public const nint SEEK_SET = 0;
		public const nint SEEK_CUR = 1;
		public const nint SEEK_END = 2;

		public const nint O_ACCMODE        = 0x00000003;
		public const nint O_RDONLY         = 0x00000000;
		public const nint O_WRONLY         = 0x00000001;
		public const nint O_RDWR           = 0x00000002;
		public const nint O_CREAT          = 0x00000040;
		public const nint O_EXCL           = 0x00000080;
		public const nint O_NOCTTY         = 0x00000100;
		public const nint O_TRUNC          = 0x00000200;
		public const nint O_APPEND         = 0x00000400;
		public const nint O_NONBLOCK       = 0x00000800;
		public const nint O_NDELAY         = 0x00000800;
		public const nint O_SYNC           = 0x00101000;
		public const nint O_ASYNC          = 0x00002000;

		[DllImport("libc.so.6", SetLastError = true)]
		public unsafe static extern nint open(string fileName, nint mode);

		[DllImport("libc.so.6", SetLastError = true)]
		public unsafe static extern nint close(nint fd);

		[DllImport("libc.so.6", SetLastError = true)]
		public unsafe static extern nint ioctl(nint fd, UInt32 request, ref ifreq arg);

		[DllImport("libc.so.6", SetLastError = true)]
		public unsafe static extern nint ioctl(nint fd, UInt32 request, void* arg);

		[DllImport("libexplain", CharSet = CharSet.Ansi)]
		// [return: MarshalAs(UnmanagedType.LPStr)]
		public unsafe static extern IntPtr explain_ioctl(nint fd, UInt32 request, void* arg);

		[DllImport("libc.so.6", SetLastError = true)]
		internal unsafe static extern nint read(nint handle, byte[] data, nuint length);

		[DllImport("libc.so.6", SetLastError = true)]
		internal unsafe static extern nint write(nint handle, byte[] data, nint length);

		[DllImport("libc.so.6", SetLastError = true)]
		internal unsafe static extern nint write(nint handle, [MarshalAs(UnmanagedType.LPStr)] string data, nint length);

		[DllImport("libc.so.6", SetLastError = true)]
		public unsafe static extern off_t lseek(nint fd, off_t offset, nint whence);

		[DllImport("libc.so.6", SetLastError = true)]
		public unsafe static extern nint poll(ref pollfd fds, UInt32 nfds, nint timeout);

		[DllImport("libc.so.6", SetLastError = true)]
		public unsafe static extern nint poll([In][Out] pollfd[] fds, UInt32 nfds, nint timeout);
		#endregion
		#region Poll
		public const nint POLLIN     = 0x001;		/* There is data to read. */
		public const nint POLLPRI    = 0x002;		/* There is urgent data to read. */
		public const nint POLLOUT    = 0x004;		/* Writing now will not block. */
		public const nint POLLRDNORM = 0x040;		/* Normal data may be read. */
		public const nint POLLRDBAND = 0x080;		/* Priority data may be read. */
		public const nint POLLWRNORM = 0x100;		/* Writing now will not block. */
		public const nint POLLWRBAND = 0x200;		/* Priority data may be written. */
		public const nint POLLMSG    = 0x400;
		public const nint POLLREMOVE = 0x1000;
		public const nint POLLRDHUP  = 0x2000;
		public const nint POLLERR    = 0x008;		/* Error condition. */
		public const nint POLLHUP    = 0x010;		/* Hung up. */
		public const nint POLLNVAL   = 0x020;		/* Invalid polling request. */

		[StructLayout(LayoutKind.Sequential)]
		public struct pollfd
		{
			public nint fd;			/* File descriptor to poll. */
			public UInt16 events;		/* Types of events poller cares about. */
			public UInt16 revents;		/* Types of events that actually occurred. */
		};
		#endregion
		#region Net
		public const int IFNAMSIZ = 16;
		public static readonly UInt32 TUNSETIFF = IOW('T', 202, sizeof(int));

		public const UInt16 IFF_TUN          = 0x0001;
		public const UInt16 IFF_TAP          = 0x0002;
		public const UInt16 IFF_NO_PI        = 0x1000;
		public const UInt16 IFF_ONE_QUEUE    = 0x2000;
		public const UInt16 IFF_VNET_HDR     = 0x4000;
		public const UInt16 IFF_TUN_EXCL     = 0x8000;
		public const UInt16 IFF_MULTI_QUEUE  = 0x0100;
		public const UInt16 IFF_ATTACH_QUEUE = 0x0200;
		public const UInt16 IFF_DETACH_QUEUE = 0x0400;
		public const UInt16 IFF_PERSIST      = 0x0800;
		public const UInt16 IFF_NOFILTER     = 0x1000;

		[StructLayout(LayoutKind.Explicit)]
		public unsafe struct ifreq
		{
			[MarshalAs(UnmanagedType.ByValTStr, SizeConst=IFNAMSIZ)]
			[FieldOffset(0)]
			public string ifr_name;

			[FieldOffset(IFNAMSIZ)]	public short ifr_flags;
			[FieldOffset(IFNAMSIZ)]	public sockaddr ifru_addr;
			[FieldOffset(IFNAMSIZ)]	public sockaddr ifru_dstaddr;
			[FieldOffset(IFNAMSIZ)]	public sockaddr ifru_broadaddr;
			[FieldOffset(IFNAMSIZ)]	public sockaddr ifru_netmask;
			[FieldOffset(IFNAMSIZ)]	public sockaddr ifru_hwaddr;
			[FieldOffset(IFNAMSIZ)]	public short ifru_flags;
			[FieldOffset(IFNAMSIZ)]	public nint ifru_ivalue;
			[FieldOffset(IFNAMSIZ)]	public nint ifru_mtu;

			// [FieldOffset(IFNAMSIZ)] struct ifmap ifru_map;

			// [MarshalAs(UnmanagedType.ByValTStr, SizeConst=IFNAMSIZ)]
			// [FieldOffset(IFNAMSIZ)]
			// public string ifru_slave;	/* Just fits the size */

			// [FieldOffset(IFNAMSIZ)]
			// [MarshalAs(UnmanagedType.ByValTStr, SizeConst=IFNAMSIZ)]
			// public string ifru_newname;

			// [FieldOffset(IFNAMSIZ)] void* ifru_data;
			// [FieldOffset(IFNAMSIZ)] struct if_settings ifru_settings;
		};

		[StructLayout(LayoutKind.Explicit)]
		public unsafe struct in_addr
		{
			[FieldOffset(0)] public in_addr_t s_addr;
		}

		[StructLayout(LayoutKind.Explicit)]
		public unsafe struct in6_addr
		{
			[FieldOffset(0)] public fixed byte  addr8[16];
			[FieldOffset(0)] public fixed UInt16 addr16[8];
			[FieldOffset(0)] public fixed UInt32 addr32[4];
		}

		[StructLayout(LayoutKind.Sequential)]
		public unsafe struct sockaddr_in
		{
			public sa_family_t sin_family;
			public in_addr sin_addr;
		}

		[StructLayout(LayoutKind.Sequential)]
		public unsafe struct sockaddr_in6
		{
			public sa_family_t sin6_family;
			public in_port_t   sin6_port;		/* Transport layer port # */
			public UInt32      sin6_flowinfo;	/* IPv6 flow information */
			public in6_addr    sin6_addr;		/* IPv6 address */
			public UInt32      sin6_scope_id;	/* IPv6 scope-id */
		}

		[StructLayout(LayoutKind.Sequential)]
		public unsafe struct sockaddr
		{
			public UInt16 sa_family;
			private fixed byte sa_data[14];
		}
		#endregion
		#region Spi
		public const uint SPI_CPHA      = 0x01;
		public const uint SPI_CPOL      = 0x02;

		public const uint SPI_MODE_0    = (0|0);
		public const uint SPI_MODE_1    = (0|SPI_CPHA);
		public const uint SPI_MODE_2    = (SPI_CPOL|0);
		public const uint SPI_MODE_3    = (SPI_CPOL|SPI_CPHA);

		public const uint SPI_CS_HIGH   = 0x04;
		public const uint SPI_LSB_FIRST = 0x08;
		public const uint SPI_3WIRE     = 0x10;
		public const uint SPI_LOOP      = 0x20;
		public const uint SPI_NO_CS     = 0x40;
		public const uint SPI_READY     = 0x80;
		public const uint SPI_TX_DUAL   = 0x100;
		public const uint SPI_TX_QUAD   = 0x200;
		public const uint SPI_RX_DUAL   = 0x400;
		public const uint SPI_RX_QUAD   = 0x800;

		/* Read / Write of SPI mode (SPI_MODE_0..SPI_MODE_3) (limited to 8 bits) */
		public static uint SPI_IOC_RD_MODE = IOR('k', 1, sizeof(byte));
		public static uint SPI_IOC_WR_MODE = IOW('k', 1, sizeof(byte));

		/* Read / Write SPI bit justification */
		public static uint SPI_IOC_RD_LSB_FIRST = IOR('k', 2, sizeof(byte));
		public static uint SPI_IOC_WR_LSB_FIRST = IOW('k', 2, sizeof(byte));

		/* Read / Write SPI device word length (1..N) */
		public static uint SPI_IOC_RD_BITS_PER_WORD = IOR('k', 3, sizeof(byte));
		public static uint SPI_IOC_WR_BITS_PER_WORD = IOW('k', 3, sizeof(byte));

		/* Read / Write SPI device default max speed hz */
		public static uint SPI_IOC_RD_MAX_SPEED_HZ = IOR('k', 4, sizeof(UInt32));
		public static uint SPI_IOC_WR_MAX_SPEED_HZ = IOW('k', 4, sizeof(UInt32));

		/* Read / Write of the SPI mode field */
		public static uint SPI_IOC_RD_MODE32 = IOR('k', 5, sizeof(UInt32));
		public static uint SPI_IOC_WR_MODE32 = IOW('k', 5, sizeof(UInt32));

		[StructLayout(LayoutKind.Sequential)]
		public struct spi_ioc_transfer {
			public UInt64 tx_buf;
			public UInt64 rx_buf;
			public UInt32 len;
			public UInt32 speed_hz;
			public UInt16 delay_usecs;
			public byte   bits_per_word;
			public byte   cs_change;
			public byte   tx_nbits;
			public byte   rx_nbits;
			public UInt16 pad;
		};

		public unsafe uint SPI_MSGSIZE(uint n)
		{
			const int IOC_SIZEBITS = 14;

			// return (n * Marshal.SizeOf(spi_ioc_transfer)) < (1 << IOC_SIZEBITS) ?
			//        (n * Marshal.SizeOf(spi_ioc_transfer)) : 0;

			return (n * (uint)sizeof(spi_ioc_transfer)) < (1u << IOC_SIZEBITS) ?
			       (n * (uint)sizeof(spi_ioc_transfer)) : 0;
		}

		public unsafe uint SPI_IOC_MESSAGE(uint n)
		{
			return IOW('k', 0, SPI_MSGSIZE(n));
		}
		#endregion
		#region Misc
		internal const UInt32 IOC_NONE  = 0;
		internal const UInt32 IOC_WRITE = 1;
		internal const UInt32 IOC_READ  = 2;

		internal static UInt32 IO(UInt32 type, UInt32 number)
		{
			return IOC(IOC_NONE, type, number, 0);
		}

		internal static UInt32 IOR(UInt32 type, UInt32 number, UInt32 size)
		{
			return IOC(IOC_READ, type, number, size);
		}

		internal static UInt32 IOW(UInt32 type, UInt32 number, UInt32 size)
		{
			return IOC(IOC_WRITE, type, number, size);
		}

		internal static UInt32 IORW(UInt32 type, UInt32 number, UInt32 size)
		{
			return IOC(IOC_READ | IOC_WRITE, type, number, size);
		}

		internal static UInt32 IOC(UInt32 dir, UInt32 type, UInt32 nr, UInt32 size)
		{
			const int _IOC_NRBITS   = 8;
			const int _IOC_TYPEBITS = 8;
			const int _IOC_SIZEBITS = 14;

			const int _IOC_NRSHIFT   = 0;
			const int _IOC_TYPESHIFT = _IOC_NRSHIFT+_IOC_NRBITS;
			const int _IOC_SIZESHIFT = _IOC_TYPESHIFT+_IOC_TYPEBITS;
			const int _IOC_DIRSHIFT  = _IOC_SIZESHIFT+_IOC_SIZEBITS;

			return (((dir)  << _IOC_DIRSHIFT)  |
	 		        ((type) << _IOC_TYPESHIFT) |
	 		        ((nr)   << _IOC_NRSHIFT)   |
	 		        ((size) << _IOC_SIZESHIFT));
		}
		#endregion

		private class PinnedBuffer : IDisposable
		{
			public GCHandle Handle { get; }
			public byte[] Data { get; private set; }
			public nuint Length { get => (nuint)Data.Length; }
			public IntPtr Ptr { get { return Handle.AddrOfPinnedObject(); }}

			public byte this[int index]
			{
				get => Data[index];
				set => Data[index] = value;
			}

			public PinnedBuffer(int size)
			{
				Data   = new byte[size];
				Handle = GCHandle.Alloc(Data, GCHandleType.Pinned);
			}

			public void Dispose()
			{
				Dispose(true);
				GC.SuppressFinalize(this);
			}

			protected virtual void Dispose(bool disposing)
			{
				if(disposing)
				{
					Handle.Free();
				}
			}
		}
	}
}
