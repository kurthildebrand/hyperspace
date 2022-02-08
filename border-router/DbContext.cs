/************************************************************************************************//**
 * @file		DbContext.cs
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
using Hyperspace.Models;
using Microsoft.EntityFrameworkCore;
// using Microsoft.EntityFrameworkCore.ChangeTracking;
using Microsoft.EntityFrameworkCore.Storage.ValueConversion;
using System;
using System.Net;

namespace Hyperspace
{
	public class HyperspaceDbContext : DbContext
	{
		public DbSet<HyperspaceDevice> Devices { get; set; }
		public DbSet<HyperspaceReport> Reports { get; set; }

		public HyperspaceDbContext(DbContextOptions<HyperspaceDbContext> options) : base(options)
		{
		}

		protected override void OnModelCreating(ModelBuilder builder)
		{
			var ipconverter = new ValueConverter<string, IPAddress>(
				todb   => IPAddress.Parse(todb),
				fromdb => fromdb.ToString());

			builder.HasPostgresExtension("postgis")
				.HasPostgresExtension("postgis_sfcgal");

			builder.Entity<HyperspaceDevice>(entity =>
			{
				entity.HasKey(d => d.ip);

				entity.ToTable("devices");

				entity.Property(d => d.updated_at)
					.HasColumnType("timestamp with time zone");

				entity.Property(d => d.ip)
					.HasColumnType("inet")
					.HasConversion(ipconverter);

				entity.HasKey(d => d.ip)
					.HasName("devices_pkey");
			});

			builder.Entity<HyperspaceReport>(entity =>
			{
				entity.HasNoKey();

				entity.ToTable("reports");

				entity.Property(r => r.ip)
					.HasColumnType("inet")
					.HasConversion(ipconverter);

				entity.Property(r => r.report)
					.IsRequired()
					.HasColumnType("jsonb")
					.HasDefaultValueSql("'{}'::jsonb");

				entity.Property(r => r.updated_at)
					.HasColumnType("timestamp with time zone");

				entity.Property(r => r.loc)
					.HasColumnType("geometry(PointZ)");

				entity.HasOne(d => d.IpNavigation)
					.WithMany()
					.HasForeignKey(d => d.ip);
			});
		}
	}
}
