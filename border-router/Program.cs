/************************************************************************************************//**
 * @file		Program.cs
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
using Microsoft.AspNetCore.Builder;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.HttpOverrides;
using Microsoft.AspNetCore.Http.Json;
using Microsoft.EntityFrameworkCore;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.OpenApi.Models;
using Npgsql;
using Hyperspace;
using Hyperspace.Hubs;
using NetTopologySuite;
using System.Text.Json.Serialization;
using System.Text.Json;


var builder = WebApplication.CreateBuilder(args);

NpgsqlConnection.GlobalTypeMapper.UseNetTopologySuite();

builder.Services.Configure<DatabaseOptions>(builder.Configuration.GetSection("ConnectionStrings"));

builder.Services.AddDbContext<HyperspaceDbContext>(options =>
	options.UseNpgsql(builder.Configuration.GetConnectionString("HyperspaceConnectionString"),
		options => options.UseNetTopologySuite()));

builder.Services.AddRazorPages();
builder.Services.AddSignalR();
builder.Services.Configure<JsonOptions>(options => {
	options.SerializerOptions.NumberHandling =
		JsonNumberHandling.AllowNamedFloatingPointLiterals |
		JsonNumberHandling.AllowReadingFromString;
	options.SerializerOptions.Converters.Add(
		new NetTopologySuite.IO.Converters.GeoJsonConverterFactory());
	// options.JsonSerializerOptions.Converters.Add(
	// 	new JsonStringEnumConverter());
});

builder.Services.AddSingleton(NtsGeometryServices.Instance);
builder.Services.AddControllers();
builder.Services.AddEndpointsApiExplorer();
builder.Services.AddSwaggerGen();

builder.Services.AddSingleton<FirmwareService>();
builder.Services.AddHostedService<FirmwareService>(
	provider => provider.GetService<FirmwareService>());

builder.Services.AddSingleton<BorderRouter>();
builder.Services.AddHostedService<BorderRouter>(
	provider => provider.GetService<BorderRouter>());


var app = builder.Build();	/* Microsoft.AspNetCore.Builder.WebApplication */

// app.Urls.Add("http://*:5000;https://*:5001");
app.Urls.Add("http://*:5000");

/* Any component that depends on the scheme, such as authentication, link generation,
 * redirects, and geolocation, must be placed after invoking the Forwarded Headers
 * Middleware. Forwarded Headers Middleware should run before other middleware. This
 * ordering ensures that the middleware relying on forwarded headers information can
 * consume the header values for processing. */
app.UseForwardedHeaders(new ForwardedHeadersOptions
{
	ForwardedHeaders = ForwardedHeaders.XForwardedFor | ForwardedHeaders.XForwardedProto
});

app.UseSwagger();
app.UseSwaggerUI(c =>
{
	c.SwaggerEndpoint("/swagger/v1/swagger.json", "Hyperspace Border Router v1");
});

if(app.Environment.IsDevelopment())
{
	app.UseDeveloperExceptionPage();
}
else
{
	app.UseExceptionHandler("/Error");
	app.UseHsts();
}

/* Todo: fix ios app SSL:
	* 		App Transport Security Settings
	* 			Allow Arbitrary Loads YES
	*	was added to fix SSL self-signed dev cert issues. */
// app.UseHttpsRedirection();
app.UseStaticFiles(new StaticFileOptions
{
	ServeUnknownFileTypes = true,
});

app.UseDefaultFiles();
app.UseRouting();
// app.UseAuthorization();
app.MapRazorPages();

app.Services.GetService<BorderRouter>().SetupApi(app);

app.UseEndpoints(endpoints =>
{
	endpoints.MapRazorPages();
	endpoints.MapControllers();
	endpoints.MapHub<DbStreamHub>("/s/stream");
	endpoints.MapHub<FirmwareUpdateHub>("/s/firmware");
});

app.Run();


public class DatabaseOptions
{
	/* Property names must match that in appsettings.json */
	public string HyperspaceConnectionString { get; set; }
}
