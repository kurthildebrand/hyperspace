/************************************************************************************************//**
 * @file		Worldmap.cs
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
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Web;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Logging;
using MathNet.Numerics.LinearAlgebra;
using MathNet.Numerics.LinearAlgebra.Double;
using System.Text.Json;

namespace Hyperspace.Controllers
{
	[ApiController]
	[Route("api/worldmap")]
	public class WorldmapController : ControllerBase
	{
		public class CalibPoint
		{
			public string IP { get; set; }
			public List<double> reportedLocation { get; set; }
			public List<double> worldLocation { get; set; }
		}

		public class CalibResult
		{
			public double[][] rotation { get; set; } =
			{
				new double[3],
				new double[3],
				new double[3],
			};
			public double[] translation { get; set; } = new double[3];
			public double scale { get; set; }

			public CalibResult(Matrix<double> R, Vector<double> t, double c)
			{
				for(int i = 0; i < 3; i++)
				{
					for(int j = 0; j < 3; j++)
					{
						rotation[i][j] = R[i,j];
					}
				}

				for(int i = 0; i < 3; i++)
				{
					translation[i] = t[i];
				}

				scale = c;
			}
		}

		/* Routes:
		 * 	worldmap/ios    AR session state for iOS apps
		 * 	worldmap/obj    Worldmap as OBJ file
		 * 	worldmap/calib  Worldmap calibration information */
		private ILogger<WorldmapController> logger { get; set; }
		private static string wm_dir = "worldmap";
		private static string wm_ios = "worldmap.ios";
		private static string wm_obj = "worldmap.obj";
		private static string wm_calib_file = "calib.json";

		private static string wm_ios_path = $"{wm_dir}/{wm_ios}";
		private static string wm_obj_path = $"{wm_dir}/{wm_obj}";
		private static string wm_calib_path = $"{wm_dir}/{wm_calib_file}";

		public WorldmapController(ILogger<WorldmapController> logger)
		{
			this.logger = logger;

			if(!Directory.Exists(wm_dir))
			{
				Directory.CreateDirectory(wm_dir);
			}
		}

		[HttpGet("{target}")]
		public ActionResult GetWorldmap(string target)
		{
			switch(target)
			{
				case "ios":
					logger.LogInformation("Get iOS worldmap");
					return GetFile(wm_ios_path, "application/octet-stream");

				case "obj":
					logger.LogInformation("Get obj worldmap");
					return GetFile(wm_obj_path, "text/plain");

				default: return NotFound();
			}
		}

		[HttpPost("{target}")]
		public async Task<IActionResult> PostWorldmap(IFormFile file, string target)
		{
			switch(target)
			{
				case "ios":
					logger.LogInformation("Post iOS worldmap");
					return await SaveFile(file, wm_ios_path);

				case "obj":
					logger.LogInformation("Post obj worldmap");
					return await SaveFile(file, wm_obj_path);

				default: return NotFound();
			}
		}

		[HttpGet("calib")]
		public ActionResult GetCalib()
		{
			return GetFile(wm_calib_path, "application/json");
		}

		[HttpPost("calib")]
		public async Task<IActionResult> PostCalib([FromBody] List<CalibPoint> points)
		{
			/* Todo: verify that there are at least 3 points */

			logger.LogInformation($"Post calib");

			var P = Matrix.Build.DenseOfRowArrays(
				points.Select(p => p.reportedLocation.ToArray()).ToArray()
			);

			var Q = Matrix.Build.DenseOfRowArrays(
				points.Select(p => p.worldLocation.ToArray()).ToArray()
			);

			(var R, var t, var c) = Kabsch(P, Q);

			/* P -> Q = (c*R*P' + t)' */
			var corrected = c * R * P.Transpose();

			for(int i = 0; i < corrected.ColumnCount; i++)
			{
				corrected[0,i] += t[0];
				corrected[1,i] += t[1];
				corrected[2,i] += t[2];
			}

			var error = Q - corrected.Transpose();

			var result = new CalibResult(R, t, c);

			await using(var stream = System.IO.File.Create(wm_calib_path))
			{
				await JsonSerializer.SerializeAsync(stream, result);
			}

			return Ok(result);
		}

		private ActionResult GetFile(string path, string content_type)
		{
			if(!System.IO.File.Exists(path))
			{
				return NotFound();
			}

			return File(System.IO.File.OpenRead(path), content_type);
		}

		private async Task<IActionResult> SaveFile(IFormFile file, string path)
		{
			long size = file.Length;

			using(var stream = System.IO.File.Create(path))
			{
				logger.LogInformation($"Saved worldmap to {path}, size {size}");
				await file.CopyToAsync(stream);
			}

			return Ok(new { size });
		}

		public static (Matrix<double> R, Vector<double> t, double c) Kabsch(Matrix<double> P, Matrix<double> Q)
		{
			/* Ref: https://zpl.fi/aligning-point-patterns-with-kabsch-umeyama-algorithm/ */

			/* Compute the centroid of each matrix */
			var Pcentroid = P.ColumnSums() / P.RowCount;
			var Qcentroid = Q.ColumnSums() / Q.RowCount;

			/* Center each matrix at the origin */
			var P0 = new DenseMatrix(P.RowCount, P.ColumnCount);
			var Q0 = new DenseMatrix(Q.RowCount, Q.ColumnCount);

			for(int i = 0; i < P.RowCount; i++)
			{
				P0[i,0] = P[i,0] - Pcentroid[0];
				P0[i,1] = P[i,1] - Pcentroid[1];
				P0[i,2] = P[i,2] - Pcentroid[2];
			}

			for(int i = 0; i < Q.RowCount; i++)
			{
				Q0[i,0] = Q[i,0] - Qcentroid[0];
				Q0[i,1] = Q[i,1] - Qcentroid[1];
				Q0[i,2] = Q[i,2] - Qcentroid[2];
			}

			var varq = Q0.RowNorms(2.0).PointwisePower(2).Sum() / Q0.RowCount;

			/* Compute the rotation matrix. Note: The nodes may generate axes that are not
			 * right-handed. Therefore, don't perform the right-handed correction from the "proper"
			 * Kabsch-Umeyama algorithm. */
			var H   = Q0.Transpose() * P0 / P.RowCount;
			var svd = H.Svd();
			var c   = varq / svd.S.Sum();
			var R   = svd.U * svd.VT;

			/* Note: this is the "proper" way to do Kabsch-Umeyama */
			// var H   = Q0.Transpose() * P0 / P.RowCount;
			// var svd = H.Svd();
			// var d   = (double)(Math.Sign(svd.U.Determinant() * svd.VT.Determinant()));
			// var S   = DenseMatrix.OfDiagonalArray(new double[] {1.0, 1.0, d});
			// var c   = varq / (DenseMatrix.OfDiagonalVector(svd.S) * S).Trace();
			// var R   = svd.U * S * svd.VT;

			var t = Qcentroid - c * R * Pcentroid;

			return (R,t,c);
		}
	}
}
