using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Mvc;
using Microsoft.AspNetCore.Mvc.RazorPages;
using Microsoft.Extensions.Logging;

namespace Hyperspace.Pages
{
	public class ReportsModel : PageModel
	{
		private readonly ILogger<ReportsModel> _logger;

		public ReportsModel(ILogger<ReportsModel> logger)
		{
			_logger = logger;
		}

		public void OnGet()
		{

		}
	}
}
