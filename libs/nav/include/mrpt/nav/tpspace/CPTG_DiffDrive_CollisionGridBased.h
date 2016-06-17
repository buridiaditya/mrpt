/* +---------------------------------------------------------------------------+
   |                     Mobile Robot Programming Toolkit (MRPT)               |
   |                          http://www.mrpt.org/                             |
   |                                                                           |
   | Copyright (c) 2005-2016, Individual contributors, see AUTHORS file        |
   | See: http://www.mrpt.org/Authors - All rights reserved.                   |
   | Released under BSD License. See details in http://www.mrpt.org/License    |
   +---------------------------------------------------------------------------+ */
#pragma once

#include <mrpt/nav/tpspace/CParameterizedTrajectoryGenerator.h>
#include <mrpt/utils/CDynamicGrid.h>
#include <mrpt/math/CPolygon.h>
#include <mrpt/utils/TEnumType.h>

namespace mrpt
{
namespace nav
{
	/** \addtogroup nav_tpspace
	  * @{  */

	/** Trajectory points in C-Space for non-holonomic robots \sa CPTG_DiffDrive_CollisionGridBased */
	struct NAV_IMPEXP TCPoint
	{
		TCPoint() {}
		TCPoint(const float	x_,const float	y_,const float	phi_,
				const float	t_,const float	dist_,
				const float	v_,const float	w_) :
			x(x_), y(y_), phi(phi_), t(t_), dist(dist_), v(v_), w(w_)
		{}
		float x, y, phi,t, dist,v,w;
	};
	typedef std::vector<TCPoint> TCPointVector;
	mrpt::utils::CStream NAV_IMPEXP & operator << (mrpt::utils::CStream& o, const mrpt::nav::TCPoint & p);
	mrpt::utils::CStream NAV_IMPEXP & operator >> (mrpt::utils::CStream& i, mrpt::nav::TCPoint & p);

	/** Base class for all PTGs suitable to non-holonomic, differentially-driven (or Ackermann) vehicles
	  * based on numerical integration of the trajectories and collision look-up-table.
	  */
	class NAV_IMPEXP CPTG_DiffDrive_CollisionGridBased : 
		public CParameterizedTrajectoryGenerator, 
		public CPTG_RobotShape_Polygonal
	{
	public:
		/** The main method to be implemented in derived classes: it defines the differential-driven differential equation */
		virtual void ptgDiffDriveSteeringFunction( float alpha, float t, float x, float y, float phi, float &v, float &w) const = 0;

		/** @name Virtual interface of each PTG implementation 
		 *  @{ */
		// getDescription(): remains to be defined in derived classes.
		// setParams() to be defined in derived classses.
		
		/** The default implementation in this class relies on a look-up-table. Derived classes may redefine this to closed-form expressions, when they exist.
		  * See full docs in base class CParameterizedTrajectoryGenerator::inverseMap_WS2TP() */
		virtual bool inverseMap_WS2TP(double x, double y, int &out_k, double &out_d, double tolerance_dist = 0.10) const MRPT_OVERRIDE;
		
		/** In this class, `out_action_cmd` contains: [0]: linear velocity (m/s),  [1]: angular velocity (rad/s). 
		  * See more docs in CParameterizedTrajectoryGenerator::directionToMotionCommand() */
		virtual void directionToMotionCommand( uint16_t k, std::vector<double> &out_action_cmd ) const MRPT_OVERRIDE;

		virtual void renderPathAsSimpleLine(const uint16_t k,mrpt::opengl::CSetOfLines &gl_obj,const float decimate_distance = 0.1f,const float max_path_distance = 0.0f) const MRPT_OVERRIDE;

		bool debugDumpInFiles(const std::string &ptg_name) const MRPT_OVERRIDE;
		
		/** Launches an exception in this class: it is not allowed in numerical integration-based PTGs to change the reference distance 
		  * after initialization. */
		virtual void setRefDistance(const double refDist);

		/** This this family of PTGs, this method builds the collision grid or load it from a cache file.
		  * Collision grids must be calculated before calling getTPObstacle().
		  * Robot shape must be set before initializing with setRobotShape(). 
		  * The rest of PTG parameters should have been set at the constructor.
		  *  \param cacheFilename The filename where the collision grids will be dumped to speed-up future recalculations. If it exists upon call, the collision grid will be loaded from here if all PTG parameters match. Example: "PTG_%03d.dat.gz".
		  */
		void initialize(const std::string & cacheFilename = std::string(), const bool verbose = true) MRPT_OVERRIDE;
		void deinitialize() MRPT_OVERRIDE;

		// Access to PTG paths (see docs in base class)
		size_t getPathStepCount(uint16_t k) const MRPT_OVERRIDE;
		void getPathPose(uint16_t k, uint16_t step, mrpt::math::TPose2D &p) const MRPT_OVERRIDE;
		double getPathDist(uint16_t k, uint16_t step) const MRPT_OVERRIDE;
		bool getPathStepForDist(uint16_t k, double dist, uint16_t &out_step) const MRPT_OVERRIDE;

		void updateTPObstacle(double ox, double oy, std::vector<double> &tp_obstacles) const MRPT_OVERRIDE;

		/** @} */  // --- end of virtual methods

		float   getMax_V() const { return V_MAX; }
		float   getMax_W() const { return W_MAX; }

protected:
		CPTG_DiffDrive_CollisionGridBased();

		void internal_processNewRobotShape() MRPT_OVERRIDE;

		/** Possible values in "params" (those in CParameterizedTrajectoryGenerator, which is called internally, plus):
		 *   - resolution: The cell size
		 *   - v_max, w_max: Maximum robot speeds.
		 *   - shape_x{0,1,2..}, shape_y{0,1,2..}: Polygonal robot shape [Optional, can be also set via `setRobotPolygonShape()`]
		 *
		 * See docs of derived classes for additional parameters in setParams()
		 */
		void setParamsCommon(const mrpt::utils::TParameters<double> &params) MRPT_OVERRIDE;

		double V_MAX, W_MAX;
		double turningRadiusReference;
		std::vector<TCPointVector> m_trajectory;
		double                     m_resolution;

		void internal_readFromStream(mrpt::utils::CStream &in) MRPT_OVERRIDE;
		void internal_writeToStream(mrpt::utils::CStream &out) const  MRPT_OVERRIDE;

		/** Numerically solve the diferential equations to generate a family of trajectories */
		void simulateTrajectories(
				float			max_time,
				float			max_dist,
				unsigned int	max_n,
				float			diferencial_t,
				float			min_dist,
				float			*out_max_acc_v = NULL,
				float			*out_max_acc_w = NULL);

		/**  A list of all the pairs (alpha,distance) such as the robot collides at that cell.
		  *  - map key   (uint16_t) -> alpha value (k)
		  *	 - map value (float)    -> the MINIMUM distance (d), in meters, associated with that "k".
		  */
		typedef std::vector<std::pair<uint16_t,float> > TCollisionCell;

		/** An internal class for storing the collision grid  */
		class NAV_IMPEXP CColisionGrid : public mrpt::utils::CDynamicGrid<TCollisionCell>
		{
		private:
			CPTG_DiffDrive_CollisionGridBased const * m_parent;

		public:
			CColisionGrid(float x_min, float x_max,float y_min, float y_max, float resolution, CPTG_DiffDrive_CollisionGridBased* parent )
				: mrpt::utils::CDynamicGrid<TCollisionCell>(x_min,x_max,y_min,y_max,resolution),
				m_parent(parent)
			{
			}
			virtual ~CColisionGrid() { }

			bool saveToFile( mrpt::utils::CStream* fil, const mrpt::math::CPolygon & computed_robotShape ) const;	//!< Save to file, true = OK
			bool loadFromFile( mrpt::utils::CStream* fil, const mrpt::math::CPolygon & current_robotShape );	//!< Load from file,  true = OK

			/** For an obstacle (x,y), returns a vector with all the pairs (a,d) such as the robot collides */
			const TCollisionCell & getTPObstacle( const float obsX, const float obsY) const;

			/** Updates the info into a cell: It updates the cell only if the distance d for the path k is lower than the previous value:
				*	\param cellInfo The index of the cell
				* \param k The path index (alpha discreet value)
				* \param d The distance (in TP-Space, range 0..1) to collision.
				*/
			void updateCellInfo( const unsigned int icx, const unsigned int icy, const uint16_t k, const float dist );

		}; // end of class CColisionGrid

		// Save/Load from files.
		bool saveColGridsToFile( const std::string &filename, const mrpt::math::CPolygon & computed_robotShape ) const;	// true = OK
		bool loadColGridsFromFile( const std::string &filename, const mrpt::math::CPolygon & current_robotShape ); // true = OK

		CColisionGrid	m_collisionGrid; //!< The collision grid

		/** Specifies the min/max values for "k" and "n", respectively.
		  * \sa m_lambdaFunctionOptimizer
		  */
		struct TCellForLambdaFunction
		{
			TCellForLambdaFunction() :
				k_min( std::numeric_limits<uint16_t>::max() ),
				k_max( std::numeric_limits<uint16_t>::min() ),
				n_min( std::numeric_limits<uint32_t>::max() ),
				n_max( std::numeric_limits<uint32_t>::min() )
			{}

			uint16_t k_min,k_max;
			uint32_t n_min,n_max;

			bool isEmpty() const { return k_min==std::numeric_limits<uint16_t>::max();	}
		};

		/** This grid will contain indexes data for speeding-up the default, brute-force lambda function */
		mrpt::utils::CDynamicGrid<TCellForLambdaFunction>	m_lambdaFunctionOptimizer;

		void freeMemory(); //!< Free all the memory buffers
	};

	/** @} */
}
	namespace utils
	{
		// Specialization must occur in the same namespace
		MRPT_DECLARE_TTYPENAME_NAMESPACE(TCPoint,mrpt::nav)
	}
}