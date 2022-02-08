using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Mvc;
using Microsoft.AspNetCore.Mvc.RazorPages;
using Microsoft.Extensions.Logging;

namespace Hyperspace.Pages
{
	public class DevicesModel : PageModel
	{
		private readonly ILogger<DevicesModel> _logger;

		public DevicesModel(ILogger<DevicesModel> logger)
		{
			_logger = logger;
		}

		public void OnGet()
		{

		}
	}
}
